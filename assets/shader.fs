#version 330

// Input vertex attributes (from vertex shader)
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragPosition;
in vec3 fragNormal;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 pos;
uniform vec3 camera;
uniform vec4 farColor;

// Output fragment color
out vec4 finalColor;

// NOTE: Add here your custom variables

void main()
{
    // Texel color fetching from texture sampler
    vec4 texelColor = texture(texture0, fragTexCoord);

    // NOTE: Implement here your fragment shader code

    vec3 cameraNormal = normalize(camera-fragPosition);

    float fresnel = max(dot(fragNormal, cameraNormal), 0.0);



    float dist = distance(pos, fragPosition);
    dist *= 0.7;
    dist = min(dist, 1);

    // dist = dist*dist;

    finalColor = texelColor*colDiffuse*fragColor;
    finalColor = mix(finalColor, farColor, max(pow(1-fresnel, 9)*0.7-0.1, 0));
    finalColor.rgb *= dist;

    dist = distance(fragPosition, camera);

    float mixFactor = sqrt(dist);
    mixFactor *=  0.02;

    mixFactor = max(mixFactor, 0);
    mixFactor = min(mixFactor, 1);

    finalColor = mix(finalColor, farColor, mixFactor);
    finalColor.a = 1;
}