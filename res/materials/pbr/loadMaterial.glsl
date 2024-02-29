
Material loadMaterial(MaterialParams params, InterpolatedValues values, GlobalData globalData) {
    Material material;

    if (params.albedoIndex < 0) {
        material.albedo = vec4(1.0);
    } else {
        material.albedo = textureGrad(sampled2D(sampledImages, nonuniformEXT(params.albedoIndex), globalData.textureSampler), values.uvGrad.uv, values.uvGrad.ddx, values.uvGrad.ddy);
    }
    if (params.normalIndex < 0)
        material.normal = vec3(0.52, 0.52, 1);
    else
        material.normal = textureGrad(sampled2D(sampledImages, nonuniformEXT(params.normalIndex), globalData.textureSampler), values.uvGrad.uv, values.uvGrad.ddx, values.uvGrad.ddy).xyz;
    material.normal = material.normal * 2.0 - 1.0;
    material.normal = normalize(values.TBN * material.normal);

    if (params.metallicRoughnessIndex < 0) {
        material.roughness = 1.0;
        material.metallic = 0.0;
    } else {
        material.metallic = textureGrad(sampled2D(sampledImages, nonuniformEXT(params.metallicRoughnessIndex), globalData.textureSampler), values.uvGrad.uv, values.uvGrad.ddx, values.uvGrad.ddy).b;
        material.roughness = textureGrad(sampled2D(sampledImages, nonuniformEXT(params.metallicRoughnessIndex), globalData.textureSampler), values.uvGrad.uv, values.uvGrad.ddx, values.uvGrad.ddy).g;
    }

    if (params.emissiveIndex < 0) {
        material.emissive = vec3(0);
    } else {
        material.emissive = textureGrad(sampled2D(sampledImages, nonuniformEXT(params.emissiveIndex), globalData.textureSampler), values.uvGrad.uv, values.uvGrad.ddx, values.uvGrad.ddy).rgb;
    }

    material.emissiveStrength = params.emissiveStrength;


    return material;
}
