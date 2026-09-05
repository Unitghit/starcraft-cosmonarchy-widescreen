sampler2D palette : register(s1);
sampler2D world : register(s2);
sampler2D overlay : register(s3);
sampler2D nativeUi : register(s4);
float4 logicalAndWrapper : register(c0); // logical WH, wrapper texture WH
float4 worldSize : register(c1); // crop WH, world texture WH
float4 overlaySize : register(c2); // overlay texture WH
float4 nativeRects[4] : register(c3); // logical XYWH, native 640x480 sources
float4 worldFilter : register(c7); // opt-in, actual output viewport WH, reserved
float4 pointerRect : register(c8);
float4 pointerShape : register(c9);
float4 pointerSelection : register(c10);
float2 divideFloor(float2 numerator,float2 denominator)
{
    float2 q=floor(numerator/denominator);
    return q+float2(numerator.x>=(q.x+1)*denominator.x,numerator.y>=(q.y+1)*denominator.y)
        -float2(numerator.x<q.x*denominator.x,numerator.y<q.y*denominator.y);
}
float4 worldColor(float2 cell)
{
    float index = tex2Dlod(world, float4((cell + 0.5) / worldSize.zw,0,0)).r;
    return tex2Dlod(palette, float4((index * 255.0 + 0.5) / 256.0,0.5,0,0));
}
float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float2 view = uv * logicalAndWrapper.zw / logicalAndWrapper.xy;
    // Recover the output pixel grid before division. Interpolated UVs can be
    // slightly below exact sample boundaries at rational scales such as 1.5x.
    float2 outputSize = worldFilter.yz;
    float2 p = floor(view * outputSize);
    [branch] if(pointerShape.x>0 && pointerShape.y>0)
    {
        float2 n=floor((p-pointerRect.xy+0.5)/max(pointerRect.zw,0.001));
        if(all(n>=0)&&all(n<pointerShape.xy))
        {
            float4 ink=tex2Dlod(nativeUi,float4((n+float2(0,1920)+0.5)/float2(1024,2048),0,0));
            if(ink.a>0.5)return tex2Dlod(palette,float4((ink.r*255+0.5)/256,0.5,0,0));
        }
    }
    [branch] if(pointerShape.w>0.5)
    {
        float2 lo=floor(pointerSelection.xy),hi=floor(pointerSelection.zw);
        float2 thickness=max(1.0,floor(outputSize/logicalAndWrapper.xy+0.5));
        if(all(p>=lo)&&all(p<=hi)&&(any(p-lo<thickness)||any(hi-p<thickness)))
            return tex2Dlod(palette,float4((pointerShape.z+0.5)/256,0.5,0,0));
    }
    float2 wp = min(divideFloor((2.0*p+1.0)*worldSize.xy,2.0*outputSize), worldSize.xy - 1);
    float2 up = min(divideFloor((2.0*p+1.0)*logicalAndWrapper.xy,2.0*outputSize), logicalAndWrapper.xy - 1);
    float4 ui = tex2D(overlay, (up + 0.5) / overlaySize.xy);
    float index = ui.r;
    bool covered = ui.a > 0.5;
    if (ui.a <= 0.5)
    {
        [unroll] for (int layer = 0; layer < 4; ++layer)
        {
            float4 r = nativeRects[layer];
            float2 n = divideFloor(((2.0*p+1.0)*logicalAndWrapper.xy - 2.0*outputSize*r.xy)*float2(640,480),
                2.0*outputSize*max(r.zw,1.0));
            if (r.z > 0 && r.w > 0 && all(n >= 0) && all(n < float2(640,480)))
            {
                float4 ink = tex2D(nativeUi, (n + float2(0,480*layer) + 0.5) / float2(1024,2048));
                if (ink.a > 0.5) { index = ink.r; covered = true; }
            }
        }
    }
    if (covered) return tex2Dlod(palette, float4((index * 255.0 + 0.5) / 256.0,0.5,0,0));
    // Exact output-pixel coverage. During enlargement a pixel can overlap
    // at most two source cells per axis. Resolve indices before mixing RGB.
    bool2 exact = outputSize == floor(outputSize / worldSize.xy + 0.5) * worldSize.xy;
    [branch] if (worldFilter.x > 0.5 && all(outputSize >= worldSize.xy) && !all(exact))
    {
        float2 lo = divideFloor(p * worldSize.xy, outputSize);
        float2 end = (p + 1) * worldSize.xy;
        float2 hi = min(divideFloor(end - 1, outputSize), worldSize.xy - 1);
        lo = float2(exact.x ? wp.x : lo.x, exact.y ? wp.y : lo.y);
        hi = float2(exact.x ? wp.x : hi.x, exact.y ? wp.y : hi.y);
        [branch] if (any(lo != hi))
        {
            float2 weight = saturate((end - hi * outputSize) / worldSize.xy);
            float4 a = worldColor(lo);
            float4 b = lo.x == hi.x ? a : worldColor(float2(hi.x,lo.y));
            float4 top = lerp(a,b,weight.x);
            [branch] if (lo.y == hi.y) return top;
            float4 c = worldColor(float2(lo.x,hi.y));
            float4 d = lo.x == hi.x ? c : worldColor(hi);
            return lerp(top,lerp(c,d,weight.x),weight.y);
        }
    }
    return worldColor(wp);
}
