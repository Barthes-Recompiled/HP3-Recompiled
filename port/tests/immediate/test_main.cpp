// gpu/immediate.h: D3DDevice_Begin/End immediate mode, packed into a stream-0 layout.
// Build and run (UCRT64):  g++ -std=c++17 -Wall -I port/src/scda port/tests/immediate/test_main.cpp -o immediate_test
//                          ./immediate_test
#include <cstdio>
#include <cstring>
#include <vector>

#include "gpu/immediate.h"

static int g_failed = 0;
#define CHECK(c)                                                         \
    do {                                                                 \
        if (!(c)) {                                                      \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c);      \
            g_failed++;                                                  \
        }                                                                \
    } while (0)

static float F(const uint8_t* p) {
    float f;
    std::memcpy(&f, p, 4);
    return f;
}

static void test_pack_d3dcolor_is_bgra() {
    const float v[4] = {1.0f, 0.5f, 0.0f, 1.0f};
    uint8_t p[4] = {};
    CHECK(gpu::PackType(0x40, v, p));
    CHECK(p[0] == 0x00 && p[1] == 0x80 && p[2] == 0xFF && p[3] == 0xFF);
}

static void test_pack_d3dcolor_clamps() {
    const float v[4] = {2.0f, -1.0f, 0.0f, 0.0f};
    uint8_t p[4] = {};
    CHECK(gpu::PackType(0x40, v, p));
    CHECK(p[2] == 0xFF && p[1] == 0x00);
}

static void test_pack_float3() {
    const float v[4] = {1.5f, -2.0f, 3.25f, 9.0f};
    uint8_t p[16] = {};
    CHECK(gpu::PackType(0x32, v, p));
    CHECK(F(p) == 1.5f && F(p + 4) == -2.0f && F(p + 8) == 3.25f && F(p + 12) == 0.0f);
}

static void test_pack_float2h_is_xyw() {
    const float v[4] = {1.0f, 2.0f, 3.0f, 4.0f};
    uint8_t p[12] = {};
    CHECK(gpu::PackType(0x72, v, p));
    CHECK(F(p) == 1.0f && F(p + 4) == 2.0f && F(p + 8) == 4.0f);
}

static void test_pack_other_types_refused() {
    const float v[4] = {};
    uint8_t p[16] = {};
    CHECK(!gpu::PackType(0x15, v, p));  // NORMSHORT1
    CHECK(!gpu::PackType(0x34, v, p));  // PBYTE3
}

static void test_vertex_register_emits_and_registers_persist() {
    gpu::Immediate im;
    im.Begin(6);
    im.Set(3, 1, 1, 1, 1);
    im.Set(gpu::Immediate::kVertex, 0, 0, 0, 1);
    im.Set(gpu::Immediate::kVertex, 1, 0, 0, 1);  // diffuse not set again: it keeps its value
    CHECK(im.Open() && im.Prim() == 6 && im.Count() == 2);
    gpu::VertexShader l;
    l.inputs[0] = {0, 0, 0x32};
    l.inputs[3] = {0, 12, 0x40};
    l.stream_stride[0] = 16;
    std::vector<uint8_t> out;
    uint32_t bad = 0;
    CHECK(im.Pack(l, out, &bad));
    CHECK(out.size() == 32);
    CHECK(F(&out[16]) == 1.0f);                                      // second vertex x
    CHECK(out[28] == 0xFF && out[29] == 0xFF && out[30] == 0xFF && out[31] == 0xFF);  // its diffuse
}

static void test_set_without_vertex_emits_nothing() {
    gpu::Immediate im;
    im.Begin(8);
    im.Set(3, 1, 0, 0, 1);
    im.Set(9, 0.5f, 0.5f, 0, 1);
    CHECK(im.Count() == 0);
}

static void test_pack_fvf_0x144_layout() {
    // XYZRHW | DIFFUSE | TEX1 as FvfInputs lays it out: v0 FLOAT4 @0, v3 D3DCOLOR @16, v9 FLOAT2 @20, stride 28
    gpu::Immediate im;
    im.Begin(6);
    im.Set(3, 1, 1, 1, 1);
    im.Set(9, 0.0f, 1.0f, 0, 1);
    im.Set(gpu::Immediate::kVertex, -0.5f, 31.5f, 0.0f, 1.0f);
    gpu::VertexShader l;
    l.inputs[0] = {0, 0, 0x42};
    l.inputs[3] = {0, 16, 0x40};
    l.inputs[9] = {0, 20, 0x22};
    l.stream_stride[0] = 28;
    std::vector<uint8_t> out;
    uint32_t bad = 0;
    CHECK(im.Pack(l, out, &bad));
    CHECK(out.size() == 28);
    CHECK(F(&out[0]) == -0.5f && F(&out[4]) == 31.5f && F(&out[12]) == 1.0f);
    CHECK(F(&out[20]) == 0.0f && F(&out[24]) == 1.0f);
}

static void test_pack_refuses_unwritable_layout() {
    gpu::Immediate im;
    im.Begin(6);
    im.Set(gpu::Immediate::kVertex, 0, 0, 0, 1);
    gpu::VertexShader l;
    l.inputs[0] = {0, 0, 0x32};
    l.inputs[2] = {0, 12, 0x16};  // NORMPACKED3: not written by immediate mode here
    l.stream_stride[0] = 16;
    std::vector<uint8_t> out;
    uint32_t bad = 0;
    CHECK(!im.Pack(l, out, &bad));
    CHECK(bad == 0x16);
}

static void test_list_is_capped() {
    gpu::Immediate im;
    im.Begin(1);
    for (size_t i = 0; i < gpu::Immediate::kMaxVertices + 5; i++) im.Set(gpu::Immediate::kVertex, 0, 0, 0, 1);
    CHECK(im.Count() == gpu::Immediate::kMaxVertices);
    CHECK(im.Overflowed());
}

static void test_end_closes_and_clears() {
    gpu::Immediate im;
    im.Begin(6);
    im.Set(gpu::Immediate::kVertex, 0, 0, 0, 1);
    im.End();
    CHECK(!im.Open() && im.Count() == 0 && !im.Overflowed());
}

int main() {
    test_pack_d3dcolor_is_bgra();
    test_pack_d3dcolor_clamps();
    test_pack_float3();
    test_pack_float2h_is_xyw();
    test_pack_other_types_refused();
    test_vertex_register_emits_and_registers_persist();
    test_set_without_vertex_emits_nothing();
    test_pack_fvf_0x144_layout();
    test_pack_refuses_unwritable_layout();
    test_list_is_capped();
    test_end_closes_and_clears();
    std::printf("%s (%d failed)\n", g_failed ? "FAILED" : "all passed", g_failed);
    return g_failed ? 1 : 0;
}
