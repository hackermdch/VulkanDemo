#include "header.hlsli"

float4 main(Point p) : SV_TARGET
{
    return p.color;
}