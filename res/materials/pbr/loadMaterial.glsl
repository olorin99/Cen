
Material loadMaterial(MaterialParams params, InterpolatedValues values, GlobalData globalData) {
    Material material;

    if (params.albedoIndex < 0) {
        material.albedo = vec4(1.0);
    } else {
        material.albedo = textureGrad(sampler2D(sampledImages[nonuniformEXT(params.albedoIndex)], samplers[globalData.textureSampler]), values.uvGrad.uv, values.uvGrad.ddx, values.uvGrad.ddy);
    }

    return material;
}
