#pragma once
#include "Includes/SceneGlobals.slang"
#include "Includes/GlobalRHI.slang"
#include "Includes/XeGTAO.slang"

// Horizon search over the prefiltered depth pyramid; writes the noisy visibility term plus packed edges.

struct FPushConstants
{
    uint2        ViewportSize;
    FTexture2D   WorkingDepth;
    FRWTexture2D AOTerm;
    FRWTexture2D Edges;
    uint         SliceCount;
    uint         StepsPerSlice;
    uint         NoiseIndex;
    float        EffectRadius;
    float        EffectFalloffRange;
    float        SampleDistributionPower;
    float        ThinOccluderCompensation;
    float        FinalValuePower;
    float        DepthMIPSamplingOffset;
};
FPushConstants* Pass() { return GetPassArgs<FPushConstants>(); }

// The quality level is frame-uniform, so its loop counts are compile-time and the slice and step loops unroll.
[vk::constant_id(20)] const uint SPEC_GTAO_SLICES               = 9;
[vk::constant_id(21)] const uint SPEC_GTAO_STEPS                = 3;
[vk::constant_id(22)] const uint SPEC_GTAO_SQUARE_DISTRIBUTION  = 1;
[vk::constant_id(23)] const uint SPEC_GTAO_NO_THIN_COMPENSATION = 1;

// The float exponent is floor(log2(X)), and building the result from bits keeps both the log2 and the int to float conversion off the transcendental unit.
float NearestMipFromExponent(float X)
{
    const int Level = clamp(int(asuint(X) >> 23u) - 127, 0, XE_GTAO_DEPTH_MIP_LEVELS);
    return asfloat(0x4B000000u | uint(Level)) - 8388608.0;
}

// Adding and removing 1.5 * 2^23 rounds to the nearest integer with two adds, where round() is a quarter-rate conversion.
float2 RoundToPixel(float2 X)
{
    precise float2 Shifted = X + 12582912.0;
    return Shifted - 12582912.0;
}

float SampleWorkingDepth(float2 UV, float MipLevel)
{
    return Pass().WorkingDepth.SampleLevel(Samplers::PointClamp, UV, MipLevel).x;
}

void StoreVisibility(uint2 PixCoord, float Visibility)
{
    const float Packed = saturate(Visibility / XE_GTAO_OCCLUSION_TERM_SCALE);
    Pass().AOTerm[PixCoord] = float4(Packed, 0.0, 0.0, 0.0);
}

[shader("compute")]
[numthreads(XE_GTAO_NUMTHREADS_X, XE_GTAO_NUMTHREADS_Y, 1)]
void ComputeMain(uint3 DTid : SV_DispatchThreadID)
{
    if (any(DTid.xy >= Pass().ViewportSize))
    {
        return;
    }

    const FGTAOView V        = MakeGTAOView(Pass().ViewportSize);
    const uint2     PixCoord = DTid.xy;
    const float2    ScreenUV = (float2(PixCoord) + 0.5) * V.ViewportPixelSize;

    // Mip 0 of the working pyramid is the unfiltered linearized depth, so the cross reads it directly.
    const float2 GatherUV = float2(PixCoord) * V.ViewportPixelSize;
    const float4 ValuesUL = Pass().WorkingDepth.GatherRed(Samplers::PointClamp, GatherUV);
    const float4 ValuesBR = Pass().WorkingDepth.GatherRed(Samplers::PointClamp, GatherUV, int2(1, 1));

    float       ViewspaceZ = ValuesUL.y;
    const float PixLZ      = ValuesUL.x;
    const float PixTZ      = ValuesUL.z;
    const float PixRZ      = ValuesBR.z;
    const float PixBZ      = ValuesBR.x;

    const float4 EdgesLRTB = XeGTAO_CalculateEdges(ViewspaceZ, PixLZ, PixRZ, PixTZ, PixBZ);
    Pass().Edges[PixCoord] = float4(XeGTAO_PackEdges(EdgesLRTB), 0.0, 0.0, 0.0);

    const float3 CenterPos = XeGTAO_ComputeViewspacePosition(V, ScreenUV, ViewspaceZ);
    const float3 LeftPos   = XeGTAO_ComputeViewspacePosition(V, ScreenUV + float2(-1.0,  0.0) * V.ViewportPixelSize, PixLZ);
    const float3 RightPos  = XeGTAO_ComputeViewspacePosition(V, ScreenUV + float2( 1.0,  0.0) * V.ViewportPixelSize, PixRZ);
    const float3 TopPos    = XeGTAO_ComputeViewspacePosition(V, ScreenUV + float2( 0.0, -1.0) * V.ViewportPixelSize, PixTZ);
    const float3 BottomPos = XeGTAO_ComputeViewspacePosition(V, ScreenUV + float2( 0.0,  1.0) * V.ViewportPixelSize, PixBZ);

    float3 ViewspaceNormal = XeGTAO_CalculateNormal(EdgesLRTB, CenterPos, LeftPos, RightPos, TopPos, BottomPos);

    // Nudge the center toward the camera so depth quantization cannot make a texel occlude itself.
    ViewspaceZ *= 0.99999;

    const float3 PixCenterPos = XeGTAO_ComputeViewspacePosition(V, ScreenUV, ViewspaceZ);
    const float3 ViewVec      = normalize(-PixCenterPos);

    // The cross-product winding depends on the projection's handedness, so resolve it against the view.
    if (dot(ViewspaceNormal, ViewVec) < 0.0)
    {
        ViewspaceNormal = -ViewspaceNormal;
    }

    const float EffectRadius             = Pass().EffectRadius;
    const float EffectFalloffRange       = Pass().EffectFalloffRange;
    const float SampleDistributionPower  = Pass().SampleDistributionPower;
    const float ThinOccluderCompensation = Pass().ThinOccluderCompensation;

    const float FalloffRange = max(EffectFalloffRange * EffectRadius, 1e-6);
    const float FalloffFrom  = EffectRadius * (1.0 - EffectFalloffRange);
    const float FalloffMul   = -1.0 / FalloffRange;
    const float FalloffAdd   = FalloffFrom / FalloffRange + 1.0;

    const float PixelSizeAtZ       = abs(V.NDCToViewMulPixelSize.x * ViewspaceZ);
    const float ScreenspaceRadius  = EffectRadius / max(PixelSizeAtZ, 1e-8);

    // Under a pixel of coverage there is no horizon to find, and MinS below would march off screen.
    if (ScreenspaceRadius < XE_GTAO_PIXEL_TOO_CLOSE)
    {
        StoreVisibility(PixCoord, 1.0);
        return;
    }

    // The reference bakes the Y flip into its screen march; derive it so either handedness works.
    const float SignX = (V.NDCToViewMulPixelSize.x < 0.0) ? -1.0 : 1.0;
    const float SignY = (V.NDCToViewMulPixelSize.y < 0.0) ? -1.0 : 1.0;

    const float2 LocalNoise  = XeGTAO_SpatioTemporalNoise(PixCoord, Pass().NoiseIndex);
    const float  NoiseSlice  = LocalNoise.x;
    const float  NoiseSample = LocalNoise.y;

    const float MinS = XE_GTAO_PIXEL_TOO_CLOSE / ScreenspaceRadius;

    const uint SliceCount    = max(SPEC_GTAO_SLICES, 1u);
    const uint StepsPerSlice = max(SPEC_GTAO_STEPS, 1u);

    // A step's offset has length S times the screen radius, and the half octave makes the exponent's floor round like the point sampler.
    const float MipScale = exp2(log2(ScreenspaceRadius) - Pass().DepthMIPSamplingOffset + 0.5);

    // Fades the effect out where the footprint is only a few pixels wide, matching the reference.
    float Visibility = saturate((10.0 - ScreenspaceRadius) / 100.0) * 0.5;

    // Slice k sits at (k + NoiseSlice) * PI / SliceCount, so each slice direction is the last one turned by a fixed angle.
    const float  SliceAngle = XE_GTAO_PI / float(SliceCount);
    const float2 SliceTurn  = float2(cos(SliceAngle), sin(SliceAngle));
    float2       SliceDir   = float2(cos(NoiseSlice * SliceAngle), sin(NoiseSlice * SliceAngle));

    for (uint Slice = 0; Slice < SliceCount; ++Slice)
    {
        const float CosPhi = SliceDir.x;
        const float SinPhi = SliceDir.y;
        SliceDir = float2(SliceDir.x * SliceTurn.x - SliceDir.y * SliceTurn.y, SliceDir.x * SliceTurn.y + SliceDir.y * SliceTurn.x);

        const float2 Omega = float2(CosPhi * SignX, SinPhi * SignY) * ScreenspaceRadius;

        const float3 DirectionVec      = float3(CosPhi, SinPhi, 0.0);
        const float3 OrthoDirectionVec = DirectionVec - (dot(DirectionVec, ViewVec) * ViewVec);
        const float3 AxisVec           = normalize(cross(OrthoDirectionVec, ViewVec));

        const float3 ProjectedNormalVec = ViewspaceNormal - AxisVec * dot(ViewspaceNormal, AxisVec);

        const float SignNorm  = sign(dot(OrthoDirectionVec, ProjectedNormalVec));
        float       ProjNLen  = length(ProjectedNormalVec);
        const float CosNorm   = saturate(dot(ProjectedNormalVec, ViewVec) / max(ProjNLen, 1e-6));
        const float SliceN    = SignNorm * XeGTAO_FastACos(CosNorm);
        const float SinSliceN = sin(SliceN);

        // Below the horizon a sample is not "unoccluded", so the reference seeds from the tangent plane, cos(SliceN -+ PI/2).
        const float LowHorizonCos0 = -SinSliceN;
        const float LowHorizonCos1 =  SinSliceN;

        float HorizonCos0 = LowHorizonCos0;
        float HorizonCos1 = LowHorizonCos1;

        for (uint Step = 0; Step < StepsPerSlice; ++Step)
        {
            const float StepBaseNoise = float(Slice + Step * StepsPerSlice) * 0.6180339887498948482;
            const float StepNoise     = frac(NoiseSample + StepBaseNoise);

            float S = (float(Step) + StepNoise) / float(StepsPerSlice);
            S = SPEC_GTAO_SQUARE_DISTRIBUTION != 0 ? S * S : pow(S, SampleDistributionPower);
            S += MinS;

            // Stepping the mip with the offset is what keeps a wide radius off the full-res cache.
            const float MipLevel = NearestMipFromExponent(S * MipScale);

            const float2 SampleOffset = RoundToPixel(S * Omega) * V.ViewportPixelSize;

            const float2 SampleUV0 = ScreenUV + SampleOffset;
            const float2 SampleUV1 = ScreenUV - SampleOffset;

            const float3 SamplePos0 = XeGTAO_ComputeViewspacePosition(V, SampleUV0, SampleWorkingDepth(SampleUV0, MipLevel));
            const float3 SamplePos1 = XeGTAO_ComputeViewspacePosition(V, SampleUV1, SampleWorkingDepth(SampleUV1, MipLevel));

            const float3 Delta0 = SamplePos0 - PixCenterPos;
            const float3 Delta1 = SamplePos1 - PixCenterPos;
            const float  DistSq0 = max(dot(Delta0, Delta0), 1e-12);
            const float  DistSq1 = max(dot(Delta1, Delta1), 1e-12);
            const float  InvDist0 = rsqrt(DistSq0);
            const float  InvDist1 = rsqrt(DistSq1);

            // Stretching the depth axis discards samples behind the center sooner, which is the thickness heuristic.
            const float FalloffBase0 = SPEC_GTAO_NO_THIN_COMPENSATION != 0 ? DistSq0 * InvDist0
                                     : length(float3(Delta0.x, Delta0.y, Delta0.z * (1.0 + ThinOccluderCompensation)));
            const float FalloffBase1 = SPEC_GTAO_NO_THIN_COMPENSATION != 0 ? DistSq1 * InvDist1
                                     : length(float3(Delta1.x, Delta1.y, Delta1.z * (1.0 + ThinOccluderCompensation)));

            const float Weight0 = saturate(FalloffBase0 * FalloffMul + FalloffAdd);
            const float Weight1 = saturate(FalloffBase1 * FalloffMul + FalloffAdd);

            const float SHC0 = lerp(LowHorizonCos0, dot(Delta0, ViewVec) * InvDist0, Weight0);
            const float SHC1 = lerp(LowHorizonCos1, dot(Delta1, ViewVec) * InvDist1, Weight1);

            HorizonCos0 = max(HorizonCos0, SHC0);
            HorizonCos1 = max(HorizonCos1, SHC1);
        }

        // Fudge from the reference against slight over-darkening on steep slopes.
        ProjNLen = lerp(ProjNLen, 1.0, 0.05);

        const float H0 = -XeGTAO_FastACos(HorizonCos1);
        const float H1 =  XeGTAO_FastACos(HorizonCos0);

        const float IArc0 = (CosNorm + 2.0 * H0 * SinSliceN - cos(2.0 * H0 - SliceN)) / 4.0;
        const float IArc1 = (CosNorm + 2.0 * H1 * SinSliceN - cos(2.0 * H1 - SliceN)) / 4.0;

        Visibility += ProjNLen * (IArc0 + IArc1);
    }

    Visibility /= float(SliceCount);
    Visibility = pow(Visibility, Pass().FinalValuePower);

    // Total occlusion is never right for a pixel the camera can see, and it wrecks the denoise weights.
    Visibility = max(0.03, Visibility);

    StoreVisibility(PixCoord, Visibility);
}
