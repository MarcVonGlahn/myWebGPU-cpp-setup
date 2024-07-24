struct VertexInput {
	@location(0) position: vec3f,
    @location(1) normal: vec3f,
    @location(2) color: vec3f,
    @location(3) uv: vec2f, // new attribute
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
	@location(1) normal: vec3f, // <--- Add a normal output
	@location(2) uv: vec2f, // <--- Add a uv output
};

/**
 * A structure holding the value of our uniforms
 */
struct MyUniforms {
	projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
	color: vec4f,
	time: f32,
};

/**
 * A structure holding the lighting settings
 */
struct LightingUniforms {
    directions: array<vec4f, 2>,
    colors: array<vec4f, 2>,
}

const pi = 3.14159265359;

// Instead of the simple uTime variable, our uniform variable is a struct
@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;
@group(0) @binding(1) var baseColorTexture: texture_2d<f32>;
@group(0) @binding(2) var textureSampler: sampler;
@group(0) @binding(3) var<uniform> uLighting: LightingUniforms;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
	// Forward the normal
    out.normal = (uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.color = in.color;
    out.uv = in.uv;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	// Compute shading
    let normal = normalize(in.normal);
    var shading = vec3f(0.0);
    for (var i: i32 = 0; i < 2; i++) {
        let direction = normalize(uLighting.directions[i].xyz);
        let color = uLighting.colors[i].rgb;
        shading += max(0.0, dot(direction, normal)) * color;
    }
    
    // Sample texture
    let baseColor = textureSample(baseColorTexture, textureSampler, in.uv).rgb;

    // Combine texture and lighting
    let color = baseColor * shading;

    return vec4f(color, uMyUniforms.color.a);
}