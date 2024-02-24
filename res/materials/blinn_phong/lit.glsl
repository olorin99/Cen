
vec4 evalMaterial(Material material, InterpolatedValues values, GlobalData globalData) {
    vec4 albedo =  material.albedo;

    vec3 ambient = albedo.xyz * 0.05;

    const GPUCamera camera = globalData.cameraBufferRef[globalData.primaryCamera].camera;
    vec3 viewDir = normalize(camera.position - values.worldPosition);

    vec3 result = ambient;

    for (uint lightIndex = 0; lightIndex < globalData.maxLightCount; lightIndex++) {
        GPULight light = globalData.lightBufferRef[lightIndex].light;

        vec3 lightVec = vec3(0.0);
        vec3 lightDir = vec3(0.0);
        float attenuation = 1.0;
        if (light.type == 0) {
            lightDir = normalize(light.position);
        } else {
            lightVec = light.position - values.worldPosition;
            lightDir = normalize(lightVec);

            float distanceSqr = dot(lightVec, lightVec);
            float rangeSqr = light.radius * light.radius;
            float dpr = distanceSqr / max(0.0001, rangeSqr);
            dpr *= dpr;
            attenuation = clamp(1 - dpr, 0, 1.0) / max(0.0001, distanceSqr);
        }


        float diff = max(dot(lightDir, material.normal), 0.0);
        vec3 diffuse = light.colour * albedo.xyz * diff * light.intensity * attenuation;

        vec3 halfWayDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(material.normal, halfWayDir), 0.0), 32.0);
        vec3 specular = vec3(0.3) * spec * light.intensity * attenuation;

        result += diffuse + specular;
    }

    return vec4(result, albedo.a);
}
