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
	
	// Scale the object
	let S = transpose(mat4x4f(
		0.3,  0.0, 0.0, 0.0,
		0.0,  0.3, 0.0, 0.0,
		0.0,  0.0, 0.3, 0.0,
		0.0,  0.0, 0.0, 1.0,
	));

	// Translate the object
	let T = transpose(mat4x4f(
		1.0,  0.0, 0.0, 0.5,
		0.0,  1.0, 0.0, 0.0,
		0.0,  0.0, 1.0, 0.0,
		0.0,  0.0, 0.0, 1.0,
	));

	// Rotate the model in the XY plane
	let angle1 = uMyUniforms.time;
	let c1 = cos(angle1);
	let s1 = sin(angle1);
	let R1 = transpose(mat4x4f(
		 c1,  s1, 0.0, 0.0,
		-s1,  c1, 0.0, 0.0,
		0.0, 0.0, 1.0, 0.0,
		0.0,  0.0, 0.0, 1.0,
	));

	// Tilt the view point in the YZ plane
	// by three 8th of turn (1 turn = 2 pi)
	let angle2 = 3.0 * pi / 4.0;
	let c2 = cos(angle2);
	let s2 = sin(angle2);
	let R2 = transpose(mat4x4f(
		1.0, 0.0, 0.0, 0.0,
		0.0,  c2,  s2, 0.0,
		0.0, -s2,  c2, 0.0,
		0.0,  0.0, 0.0, 1.0,
	));

	// Compose and apply rotations
	// (S then T then R1 then R2, remember this reads backwards)
	let homogeneous_position = vec4f(in.position, 1.0);
	let position = (T * R2 * R1 * S * homogeneous_position).xyz;

	out.position = vec4<f32>(position.x, position.y * ratio, position.z * 0.5 + 0.5, 1.0);


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