#pragma once
#include "gr/kernel/records.hpp"

namespace gr::history {
// Pure recording-history DAG. It is independent of a storage ledger and does not
// infer represented occurrence order, target continuity or evidential authority.
// Empty bundle: {format:"gr.history/1",nodes:[]}.
Json validate(const Json &bundle, const core::TypeRegistry &registry = core::default_types());
Json append(const Json &bundle, const Json &snapshot, const Json &parents, const Json &attribution,
            const core::TypeRegistry &registry = core::default_types());
Json reconstruct(const Json &bundle, const std::string &revision,
                 const core::TypeRegistry &registry = core::default_types());
Json diff(const Json &bundle, const std::string &left, const std::string &right,
          const core::TypeRegistry &registry = core::default_types());
// Explicit-base three-way merge. Each conflicting ID needs a full validated record
// resolution or null (absence, not erasure). Unresolved conflicts return merged=false
// without changing bundle. No last-writer-wins or automatic semantic adjudication.
Json merge(const Json &bundle, const std::string &base, const std::string &left, const std::string &right,
           const Json &resolutions, const Json &attribution,
           const core::TypeRegistry &registry = core::default_types());
} // namespace gr::history
