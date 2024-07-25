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
    @location(3) viewDirection: vec3f, // <--- Add a view direction output
};

/**
 * A structure holding the value of our uniforms
 */
struct MyUniforms {
	projectionMatrix: mat4x4f,
    viewMatrix: mat4x4f,
    modelMatrix: mat4x4f,
	color: vec4f,
    cameraWorldPosition: vec3f, // new field!
	time: f32,
};

/**
 * A structure holding the lighting settings
 */
struct LightingUniforms {
    directions: array<vec4f, 2>,
    colors: array<vec4f, 2>,
	hardness: f32,
	kd: f32,
	ks: f32,
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
    let worldPosition = uMyUniforms.modelMatrix * vec4f(in.position, 1.0);
    out.position = uMyUniforms.projectionMatrix * uMyUniforms.viewMatrix * worldPosition;

    // Then we only need the camera position to get the view direction:
    let cameraWorldPosition = uMyUniforms.cameraWorldPosition;
    out.viewDirection = cameraWorldPosition - worldPosition.xyz;
	return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4f {
	// Compute shading
	let N = normalize(in.normal);
	let V = normalize(in.viewDirection);

	// Sample texture
	let baseColor = textureSample(baseColorTexture, textureSampler, in.uv).rgb;
	let kd = uLighting.kd;
	let ks = uLighting.ks;
	let hardness = uLighting.hardness;

	var color = vec3f(0.0);
	for (var i: i32 = 0; i < 2; i++) {
		let lightColor = uLighting.colors[i].rgb;
		let L = normalize(uLighting.directions[i].xyz);
		let R = reflect(-L, N); // equivalent to 2.0 * dot(N, L) * N - L

		let diffuse = max(0.0, dot(L, N)) * lightColor;

		// We clamp the dot product to 0 when it is negative
		let RoV = max(0.0, dot(R, V));
		let specular = pow(RoV, hardness);

		color += baseColor * kd * diffuse + ks * specular;
	}

    return vec4f(color, uMyUniforms.color.a);
}