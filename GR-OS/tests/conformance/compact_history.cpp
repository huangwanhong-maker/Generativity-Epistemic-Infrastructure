#include "gr/kernel/compact_history.hpp"
#include <algorithm>
#include <iostream>
#include <random>

using gr::Json;
namespace {
int checks = 0;
void check(bool c, const char *m) {
    ++checks;
    if (!c)
        throw std::runtime_error(m);
}
template <class F> void fails(F &&f, const char *m) {
    bool failed = false;
    try {
        f();
    } catch (const gr::Error &) {
        failed = true;
    }
    check(failed, m);
}
Json record(const std::string &id, int value) {
    return gr::core::construct({{"format", "gr.record/1"},
                                {"id", id},
                                {"type", "gr.assertion@1"},
                                {"payload", {{"value", value}, {"padding", std::string(80, 'x')}}},
                                {"attribution", {{"agent", "test:observer"}, {"method", "reported"}}}});
}
Json attribution(int step) {
    return {{"agent", "test:recorder"}, {"method", "checkpoint"}, {"reason", "step " + std::to_string(step)}};
}
struct Objects {
    std::map<std::string, std::vector<std::uint8_t>> bytes;
    gr::history::ObjectLoader loader() {
        return [this](const std::string &d, std::size_t limit) {
            if (!bytes.contains(d))
                throw gr::Error("not_found", "Missing fixture object");
            if (bytes.at(d).size() > limit)
                throw gr::Error("history_limit", "Fixture read limit");
            return bytes.at(d);
        };
    }
    void add(const gr::history::EncodedNode &n) {
        for (auto &o : n.objects)
            bytes.emplace(o.digest, o.bytes);
    }
};
} // namespace
int main() {
    try {
        Objects objects;
        Json bundle = {{"format", "gr.history/1"}, {"nodes", Json::array()}};
        Json initial = {{"revision", "0"}, {"records", Json::array()}};
        for (int i = 0; i < 20; ++i)
            initial["records"].push_back(record("r:" + std::to_string(i), i));
        std::vector<gr::history::NodeRef> refs;
        std::vector<Json> states;
        std::mt19937 rng(0x47524834);
        bool delta_seen = false, checkpoint_seen = false;
        bool archive_removal_seen = false;
        for (int i = 0; i < 28; ++i) {
            std::vector<gr::history::NodeRef> parents;
            Json parent_ids = Json::array(), candidate = initial;
            if (i > 0) {
                const auto p = i < 10 ? refs.size() - 1 : rng() % refs.size();
                parents.push_back(refs[p]);
                parent_ids.push_back(refs[p].revision);
                candidate = gr::core::apply(
                    states[p], Json::array({record("r:" + std::to_string(rng() % 20), i + 100)}));
                // v1 checkpoint semantics permit archive loss when explicitly supplied.
                if (i % 5 == 0)
                    candidate["archive"] = Json::array();
                if (i % 7 == 0)
                    candidate["records"].erase(candidate["records"].begin());
            }
            bundle = gr::history::append(bundle, candidate, parent_ids, attribution(i));
            const auto &node = bundle["nodes"].back();
            check(gr::history::make_node(candidate, parent_ids, attribution(i)) == node,
                  "Shared identity primitive preserves v1 node exactly");
            gr::history::CompactReader reader(objects.loader());
            auto encoded = gr::history::encode_node(
                node, parents, reader,
                parents.empty() ? std::optional<gr::history::NodeRef>{} : parents.front(), {4, 3});
            check(encoded.node.revision == node["version"].get<std::string>(),
                  "Physical representation preserves semantic version");
            check(encoded.delta_depth <= 3, "Encoding respects delta depth policy");
            delta_seen |= encoded.delta_depth > 0;
            checkpoint_seen |= i > 0 && encoded.delta_depth == 0;
            objects.add(encoded);
            refs.push_back(encoded.node);
            const auto payload = gr::history::decode_object(encoded.objects.front().bytes);
            if (payload["format"] == "gr.history-delta/1" && !payload["archive_remove"].empty())
                archive_removal_seen = true;
            gr::history::CompactReader selected(objects.loader());
            auto state = selected.reconstruct(encoded.node);
            check(state == gr::history::reconstruct(bundle, encoded.node.revision),
                  "Selected delta reconstruction equals v1 oracle");
            check(selected.stats().objects <= 8,
                  "Selected reconstruction reads at most checkpoint interval payloads and headers");
            states.push_back(state);
        }
        check(delta_seen && checkpoint_seen, "Sparse deltas and periodic checkpoints both exercised");
        check(archive_removal_seen, "Explicit archive removal survives physical delta encoding");
        gr::history::CompactReader all(objects.loader());
        auto exported = all.export_v1(refs);
        check(exported == bundle, "Complete export preserves every v1 descriptor and checkpoint exactly");
        check(all.ancestor(refs[0], refs[9]), "Small-header ancestry traverses retained parent graph");
        check(!all.ancestor(refs[9], refs[0]), "Ancestry remains directional");
        auto merge = gr::history::merge(bundle, refs[0].revision, refs[1].revision, refs[2].revision,
                                        Json::object(), attribution(100));
        check(merge["merged"], "Compatible branch endpoints merge through existing semantic oracle");
        const auto &merged_node = merge["history"]["nodes"].back();
        gr::history::CompactReader encoder(objects.loader());
        auto encoded_merge =
            gr::history::encode_node(merged_node, {refs[1], refs[2]}, encoder, refs[1], {4, 3});
        objects.add(encoded_merge);
        gr::history::CompactReader merge_reader(objects.loader());
        check(merge_reader.reconstruct(encoded_merge.node) ==
                  gr::history::reconstruct(merge["history"], merge["revision"]),
              "Two-parent merge representation preserves v1 resolution and ancestry");
        auto damaged = objects;
        damaged.bytes[refs.back().object_digest][0] ^= 1;
        gr::history::CompactReader corrupt(damaged.loader());
        fails([&] { corrupt.reconstruct(refs.back()); }, "Physical corruption rejected on fresh read");
        auto h = gr::history::decode_object(objects.bytes.at(refs[0].object_digest));
        h["document"]["attribution"]["reason"] = "changed";
        auto forged = gr::history::encode_object(h);
        objects.bytes[forged.digest] = forged.bytes;
        gr::history::CompactReader forged_reader(objects.loader());
        fails([&] { forged_reader.reconstruct({refs[0].revision, forged.digest}); },
              "Rehashed physical header cannot change semantic descriptor");
        auto delta_header = gr::history::decode_object(objects.bytes.at(refs[1].object_digest));
        auto malformed_delta =
            gr::history::decode_object(objects.bytes.at(delta_header["state"].get<std::string>()));
        check(malformed_delta["format"] == "gr.history-delta/1",
              "Malformed delta fixture uses sparse representation");
        malformed_delta["records_remove"].push_back("absent:identity");
        auto bad_payload = gr::history::encode_object(malformed_delta);
        objects.bytes[bad_payload.digest] = bad_payload.bytes;
        delta_header["state"] = bad_payload.digest;
        auto bad_header = gr::history::encode_object(delta_header);
        objects.bytes[bad_header.digest] = bad_header.bytes;
        gr::history::CompactReader malformed_reader(objects.loader());
        fails([&] { malformed_reader.reconstruct({refs[1].revision, bad_header.digest}); },
              "Self-consistently hashed malformed removal cannot corrupt reconstructed state");
        auto wrong_snapshot = gr::history::decode_object(objects.bytes.at(refs[0].object_digest));
        auto payload =
            gr::history::decode_object(objects.bytes.at(wrong_snapshot["state"].get<std::string>()));
        payload["snapshot"]["records"][0] = record("r:0", 999);
        auto changed_payload = gr::history::encode_object(payload);
        objects.bytes[changed_payload.digest] = changed_payload.bytes;
        wrong_snapshot["state"] = changed_payload.digest;
        auto changed_header = gr::history::encode_object(wrong_snapshot);
        objects.bytes[changed_header.digest] = changed_header.bytes;
        gr::history::CompactReader state_reader(objects.loader());
        fails([&] { state_reader.reconstruct({refs[0].revision, changed_header.digest}); },
              "Rehashed valid record substitution still fails semantic checkpoint identity");
        gr::history::CompactLimits limits;
        limits.read_objects = 1;
        gr::history::CompactReader bounded(objects.loader(), {}, limits);
        fails([&] { bounded.reconstruct(refs[0]); }, "Object read budget enforced");
        limits = {};
        limits.read_bytes = 10;
        gr::history::CompactReader bytes_bounded(objects.loader(), {}, limits);
        fails([&] { bytes_bounded.reconstruct(refs[0]); }, "Read byte budget enforced before fixture copy");
        std::string duplicate = "{\"x\":1,\"x\":2}";
        fails(
            [&] {
                gr::history::decode_object(std::span<const std::uint8_t>(
                    reinterpret_cast<const std::uint8_t *>(duplicate.data()), duplicate.size()));
            },
            "Strict history object decoder rejects duplicate keys");
        auto incomplete = refs;
        incomplete.erase(incomplete.begin());
        gr::history::CompactReader missing(objects.loader());
        fails([&] { missing.export_v1(incomplete); },
              "Explicit export rejects incomplete semantic ancestry inventory");
        std::cout << "compact_history: " << checks
                  << " checks passed; 28 differential branching states and merge\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "compact_history failure: " << e.what() << '\n';
        return 1;
    }
}
