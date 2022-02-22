#include "header.hlsli"

VertexOut main(VertexIn vIn)
{
    VertexOut vOut;
    vOut.pos = float4(vIn.pos, 1);
    vOut.color = vIn.color;
    return vOut;
}