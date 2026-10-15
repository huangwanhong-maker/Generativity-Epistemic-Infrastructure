#include "gr/kernel/record_operations.hpp"
#include "gr/kernel/records.hpp"
namespace gr::kernel {
void install_record_operations(ValueTypeRegistry &types, OperationRegistry &operations) {
    types.register_type(
        {"gr.record-verification@1",
         Representation::Json,
         "The qualified report returned by the GR v1 record-envelope verifier; no factual truth or authority "
         "claim.",
         {{"kind", "object"},
          {"required_keys", {"valid", "semantic_complete", "scope", "unknown_types", "errors"}}}},
        [](const Value &value) {
            const auto &j = value.as_json();
            if (!j.at("valid").is_boolean() || !j.at("semantic_complete").is_boolean() ||
                !j.at("scope").is_string() || !j.at("unknown_types").is_array() ||
                !j.at("errors").is_array() || (j.at("semantic_complete") == true && j.at("valid") != true))
                throw Error("verification_report", "Malformed qualified verification report.");
        });
    OperationDescriptor descriptor{
        "gr.record.verify@1",
        {"gr.json@1"},
        {"gr.record-verification@1"},
        Effect::Pure,
        Determinism::Exact,
        "Verify a record envelope, canonical content digest and installed default record profiles, "
        "preserving invalid and opaque reports.",
        {"The entire operand is within the bounded gr.canonical-json/1 domain."},
        {"Verification does not establish truth, signatures, authorization or historical admission."},
        {{"record_profiles", core::default_types().descriptors()},
         {"reference_behavior", "gr::core::verify"}}};
    pin_types(descriptor, types);
    operations.register_operation(std::move(descriptor));
}
} // namespace gr::kernel
