
vec3 evalLight(Material material, InterpolatedValues values, GPULight light, GPUCamera camera) {
    vec3 L = normalize(light.position - values.worldPosition);
    vec3 V = normalize(camera.position - values.world);
    vec3 H = normalize(L + V);

    float NdotV = max(dot(material.normal, V), 0.0);
    float NdotL = max(dot(material.normal, L), 0.0);
    float NdotH = max(dot(material.normal, H), 0.0);
    float LdotH = max(dot(L, H), 0.0);

}

vec4 evalMaterial(Material material, InterpolatedValues values, GlobalData globalData) {
    const GPUCamera camera = globalData.cameraBufferRef[globalData.primaryCamera].camera;

    vec3 colour = vec3(0.0);

    for (uint lightIndex = 0; lightIndex < globalData.maxLightCount; lightIndex++) {
        GPULight light = globalData.lightBufferRef[lightIndex].light;

        colour += evalLight(material, values, light, camera);
    }

    return vec4(colour, 1.0);
}
