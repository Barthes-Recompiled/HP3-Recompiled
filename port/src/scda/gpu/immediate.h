#pragma once
// D3DDevice_Begin / SetVertexData* / End immediate mode. SetVertexData sets a vertex shader input register, which
// keeps its value like the NV2A's inline attributes; writing D3DVSDE_VERTEX (0xFFFFFFFF) sets v0 and emits a vertex
// holding every register. End packs the list in the current stream-0 layout for one UP draw (hle_d3d8.cpp).
// Header-only so port/tests/immediate can build it alone.
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include "gpu/state.h"

namespace gpu {

// one register value written as D3DVSDT `type` (the inverse of vertex.cpp's Convert); false for a type this does
// not write
inline bool PackType(uint32_t type, const float v[4], uint8_t* p) {
    if (type == 0x72) {  // FLOAT2H: x, y, w
        const float h[3] = {v[0], v[1], v[3]};
        std::memcpy(p, h, 12);
        return true;
    }
    uint32_t n = (type >> 4) & 15;
    switch (type & 15) {
        case 0: {  // D3DCOLOR, bytes B G R A
            if (n != 4) return false;
            auto b = [](float f) { return uint8_t(std::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f); };
            p[0] = b(v[2]);
            p[1] = b(v[1]);
            p[2] = b(v[0]);
            p[3] = b(v[3]);
            return true;
        }
        case 2:  // FLOAT1-4 (n 0 is D3DVSDT_NONE: no stream data)
            if (n > 4) return false;
            std::memcpy(p, v, n * 4);
            return true;
        default:
            return false;
    }
}

class Immediate {
public:
    static constexpr uint32_t kVertex = 0xFFFFFFFFu;  // D3DVSDE_VERTEX
    static constexpr size_t kMaxVertices = 4096;

    void Begin(uint32_t prim) {
        prim_ = prim;
        open_ = true;
        overflow_ = false;
        verts_.clear();
    }

    void Set(uint32_t reg, float x, float y, float z, float w) {
        if (reg == kVertex) {
            regs_[0] = {x, y, z, w};
            if (verts_.size() < kMaxVertices)
                verts_.push_back(regs_);
            else
                overflow_ = true;
            return;
        }
        if (reg < regs_.size()) regs_[reg] = {x, y, z, w};
    }

    void End() {
        open_ = false;
        overflow_ = false;
        verts_.clear();
    }

    bool Open() const { return open_; }
    uint32_t Prim() const { return prim_; }
    size_t Count() const { return verts_.size(); }
    bool Overflowed() const { return overflow_; }

    // the list as stream-0 vertices of `layout`; false (and the type in *bad_type) when a register's type is one
    // PackType does not write
    bool Pack(const VertexShader& layout, std::vector<uint8_t>& out, uint32_t* bad_type) const {
        uint32_t stride = layout.stream_stride[0];
        out.assign(verts_.size() * stride, 0);
        for (size_t i = 0; i < verts_.size(); i++)
            for (size_t r = 0; r < layout.inputs.size(); r++) {
                const auto& in = layout.inputs[r];
                if (in.stream != 0) continue;
                if (!PackType(in.type, verts_[i][r].data(), &out[i * stride + in.offset])) {
                    if (bad_type) *bad_type = in.type;
                    return false;
                }
            }
        return true;
    }

private:
    using Regs = std::array<std::array<float, 4>, 16>;
    static Regs Defaults() {
        Regs r{};
        for (auto& v : r) v = {0.0f, 0.0f, 0.0f, 1.0f};
        return r;
    }
    Regs regs_ = Defaults();
    std::vector<Regs> verts_;
    uint32_t prim_ = 0;
    bool open_ = false, overflow_ = false;
};

}  // namespace gpu
