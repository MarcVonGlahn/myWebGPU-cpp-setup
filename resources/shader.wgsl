struct VertexInput {
	@location(0) position: vec3f,
	@location(1) color: vec3f,
};

struct VertexOutput {
	@builtin(position) position: vec4f,
	@location(0) color: vec3f,
    @location(1) passPos: vec2f,
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

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
	var out: VertexOutput;
	let ratio = 640.0 / 480.0;
	
	out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * uMyUniforms.modelMatrix * vec4f(in.position, 1.0);

    out.passPos = out.position.xy;
	out.color = in.color;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	 
	var color = in.color.rgb * vec3f(1.0, 1.0, 0.0);

    // So compiler doesn't act up
    let stay = 1.2 * uMyUniforms.color.rgb;
	// Gamma-correction
	let corrected_color = pow(color, vec3f(2.2));
	return vec4f(color, uMyUniforms.color.a);
}