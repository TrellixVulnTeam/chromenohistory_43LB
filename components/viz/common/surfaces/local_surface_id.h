// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_H_
#define COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_H_

#include <inttypes.h>

#include <iosfwd>
#include <string>
#include <tuple>

#include "base/hash/hash.h"
#include "base/unguessable_token.h"
#include "components/viz/common/viz_common_export.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace viz {
namespace mojom {
class LocalSurfaceIdDataView;
}

class ParentLocalSurfaceIdAllocator;
class ChildLocalSurfaceIdAllocator;

constexpr uint32_t kInvalidParentSequenceNumber = 0;
constexpr uint32_t kInvalidChildSequenceNumber = 0;
constexpr uint32_t kInitialParentSequenceNumber = 1;
constexpr uint32_t kInitialChildSequenceNumber = 1;
constexpr uint32_t kMaxParentSequenceNumber =
    std::numeric_limits<uint32_t>::max();
constexpr uint32_t kMaxChildSequenceNumber =
    std::numeric_limits<uint32_t>::max();

// This struct is the part of SurfaceId that can be modified by the client.
// LocalSurfaceId uniquely identifies a surface among the surfaces created by a
// particular client. A SurfaceId, which is FrameSinkId+LocalSurfaceId, uniquely
// identifies a surface globally across all clients.
//
// LocalSurfaceId consists of:
//
// - parent_sequence_number: This part is incremented by the embedder of the
//   client.
//
// - child_sequence_number: This part is incremented by the client itself.
//
// - embed_token: An UnguessableToken generated by the embedder. The purpose of
//   this value is to make SurfaceIds unguessable, because FrameSinkIds and
//   LocalSurfaceIds are otherwise predictable and clients might exploit this
//   fact to embed surfaces they're not allowed to. This value is generated once
//   by ParentLocalSurfaceIdAllocator and remains constant during the lifetime
//   of the embedding, even if a new LocalSurfaceId is generated for the
//   embedded client because of some change in its state (e.g. size,
//   device scale factor, etc.), or for other reasons. If a client is
//   re-parented, then the new parent allocates a new LocalSurfaceId, with a new
//   embed token, and communicates that to the embedded client.
//
// The embedder uses ParentLocalSurfaceIdAllocator to generate LocalSurfaceIds
// for the embedee. If Surface Synchronization is on, the embedee uses
// ChildLocalSurfaceIdAllocator to generate LocalSurfaceIds for itself. If
// Surface Synchronization is off, the embedee also uses
// ParentLocalSurfaceIdAllocator, as the parent doesn't generate LocalSurfaceIds
// for the child.
class VIZ_COMMON_EXPORT LocalSurfaceId {
 public:
  constexpr LocalSurfaceId()
      : parent_sequence_number_(kInvalidParentSequenceNumber),
        child_sequence_number_(kInvalidChildSequenceNumber) {}

  constexpr LocalSurfaceId(const LocalSurfaceId& other)
      : parent_sequence_number_(other.parent_sequence_number_),
        child_sequence_number_(other.child_sequence_number_),
        embed_token_(other.embed_token_) {}

  constexpr LocalSurfaceId(uint32_t parent_sequence_number,
                           const base::UnguessableToken& embed_token)
      : parent_sequence_number_(parent_sequence_number),
        child_sequence_number_(kInitialChildSequenceNumber),
        embed_token_(embed_token) {}

  constexpr LocalSurfaceId(uint32_t parent_sequence_number,
                           uint32_t child_sequence_number,
                           const base::UnguessableToken& embed_token)
      : parent_sequence_number_(parent_sequence_number),
        child_sequence_number_(child_sequence_number),
        embed_token_(embed_token) {}

  static constexpr LocalSurfaceId MaxSequenceId() {
    return LocalSurfaceId(kMaxParentSequenceNumber, kMaxChildSequenceNumber,
                          base::UnguessableToken());
  }

  constexpr bool is_valid() const {
    return parent_sequence_number_ != kInvalidParentSequenceNumber &&
           child_sequence_number_ != kInvalidChildSequenceNumber &&
           !embed_token_.is_empty();
  }

  constexpr uint32_t parent_sequence_number() const {
    return parent_sequence_number_;
  }

  constexpr uint32_t child_sequence_number() const {
    return child_sequence_number_;
  }

  constexpr const base::UnguessableToken& embed_token() const {
    return embed_token_;
  }

  // The |embed_trace_id| is used as the id for trace events associated with
  // embedding this LocalSurfaceId.
  uint64_t embed_trace_id() const { return hash() << 1; }

  // The |submission_trace_id| is used as the id for trace events associated
  // with submission of a CompositorFrame to a surface with this LocalSurfaceId.
  uint64_t submission_trace_id() const { return (hash() << 1) | 1; }

  bool operator==(const LocalSurfaceId& other) const {
    return parent_sequence_number_ == other.parent_sequence_number_ &&
           child_sequence_number_ == other.child_sequence_number_ &&
           embed_token_ == other.embed_token_;
  }

  bool operator!=(const LocalSurfaceId& other) const {
    return !(*this == other);
  }

  size_t hash() const {
    DCHECK(is_valid()) << ToString();
    return base::HashInts(
        static_cast<uint64_t>(
            base::HashInts(parent_sequence_number_, child_sequence_number_)),
        static_cast<uint64_t>(base::UnguessableTokenHash()(embed_token_)));
  }

  std::string ToString() const;

  // Returns whether this LocalSurfaceId was generated after |other|.
  bool IsNewerThan(const LocalSurfaceId& other) const;

  // Returns whether this LocalSurfaceId was generated after |other| or equal to
  // it.
  bool IsSameOrNewerThan(const LocalSurfaceId& other) const;

  // Returns the smallest valid LocalSurfaceId with the same embed token as this
  // LocalSurfaceID.
  LocalSurfaceId ToSmallestId() const;

 private:
  friend struct mojo::StructTraits<mojom::LocalSurfaceIdDataView,
                                   LocalSurfaceId>;
  friend class ParentLocalSurfaceIdAllocator;
  friend class ChildLocalSurfaceIdAllocator;

  friend bool operator<(const LocalSurfaceId& lhs, const LocalSurfaceId& rhs);

  uint32_t parent_sequence_number_;
  uint32_t child_sequence_number_;
  base::UnguessableToken embed_token_;
};

VIZ_COMMON_EXPORT std::ostream& operator<<(
    std::ostream& out,
    const LocalSurfaceId& local_surface_id);

inline bool operator<(const LocalSurfaceId& lhs, const LocalSurfaceId& rhs) {
  return std::tie(lhs.parent_sequence_number_, lhs.child_sequence_number_,
                  lhs.embed_token_) < std::tie(rhs.parent_sequence_number_,
                                               rhs.child_sequence_number_,
                                               rhs.embed_token_);
}

inline bool operator>(const LocalSurfaceId& lhs, const LocalSurfaceId& rhs) {
  return operator<(rhs, lhs);
}

inline bool operator<=(const LocalSurfaceId& lhs, const LocalSurfaceId& rhs) {
  return !operator>(lhs, rhs);
}

inline bool operator>=(const LocalSurfaceId& lhs, const LocalSurfaceId& rhs) {
  return !operator<(lhs, rhs);
}

struct LocalSurfaceIdHash {
  size_t operator()(const LocalSurfaceId& key) const { return key.hash(); }
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_SURFACES_LOCAL_SURFACE_ID_H_
