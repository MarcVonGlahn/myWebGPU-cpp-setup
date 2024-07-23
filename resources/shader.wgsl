struct VertexInput {
	@location(0) position: vec3f,
    @location(1) normal: vec3f, // new attribute
    @location(2) color: vec3f,
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

const pi = 3.14159265359;

// Instead of the simple uTime variable, our uniform variable is a struct
@group(0) @binding(0) var<uniform> uMyUniforms: MyUniforms;
@group(0) @binding(1) var gradientTexture: texture_2d<f32>;

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
	// Forward the normal
    out.normal = (uMyUniforms.modelMatrix * vec4f(in.normal, 0.0)).xyz;
	out.color = in.color;

	// In plane.obj, the vertex xy coords range from -1 to 1
    // and we remap this to the resolution-agnostic (0, 1) range
    out.uv = in.position.xy * 0.5 + 0.5;

	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	// We remap UV coords to actual texel coordinates
    let texelCoords = vec2i(in.uv * vec2f(textureDimensions(gradientTexture)));
    let color = textureLoad(gradientTexture, texelCoords, 0).rgb;

	return vec4f(color, uMyUniforms.color.a);
}