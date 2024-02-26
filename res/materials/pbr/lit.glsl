
const float PI = 3.1415926559;

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    return nom / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

vec3 evalLight(Material material, InterpolatedValues values, GPULight light, GPUCamera camera) {
    vec3 lightVec = light.position - values.worldPosition;
    vec3 L = normalize(lightVec);
    vec3 V = normalize(camera.position - values.worldPosition);
    vec3 H = normalize(L + V);

    float NdotV = max(dot(material.normal, V), 0.0);
    float NdotL = max(dot(material.normal, L), 0.0);
    float NdotH = max(dot(material.normal, H), 0.0);
    float LdotH = max(dot(L, H), 0.0);

    float attenuation = 1.0;
    if (light.type == 1) {
        float distanceSqared = dot(lightVec, lightVec);
        float rangeSquared = light.radius * light.radius;
        float dpr = distanceSqared / max(0.0001, rangeSquared);
        dpr *= dpr;
        attenuation = clamp(1 - dpr, 0, 1.0) / max(0.0001, distanceSqared);
    }

    vec3 F0 = vec3(0.04);
    F0 = mix(F0, material.albedo.rgb, material.metallic);

    vec3 radiance = light.colour * light.intensity * attenuation;

    float NDF = distributionGGX(material.normal, H, material.roughness);
    float G = geometrySmith(material.normal, V, L, material.roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 nominator = NDF * G * F;
    float denominator = 4.0 * NdotV * NdotL + 0.0001;
    vec3 specular = nominator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= (1 - material.metallic);
    return (kD * material.albedo.rgb / PI + specular) * radiance * NdotL;
}

vec4 evalMaterial(Material material, InterpolatedValues values, GlobalData globalData) {
    const GPUCamera camera = globalData.cameraBufferRef[globalData.primaryCamera].camera;

    vec3 colour = vec3(0.0);

    for (uint lightIndex = 0; lightIndex < globalData.maxLightCount; lightIndex++) {
        GPULight light = globalData.lightBufferRef[lightIndex].light;

        colour += evalLight(material, values, light, camera);
    }

    vec3 emissive = material.albedo.rgb * material.emissive * material.emissiveStrength;
    colour += emissive;

    return vec4(colour, 1.0);
}
