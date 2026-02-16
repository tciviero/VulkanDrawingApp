struct VertexOut {
	float4 pos : SV_Position;
	float2 uv : TEXCOORD0;
};

struct MouseData {
	float x;
	float y;
	float lastX;
	float lastY;
	bool erase;
	float3 color;
};

[shader("vertex")]
VertexOut vertexMain(uint vIdx: SV_VertexID) {
	VertexOut output;

	const float2 positions[6] = {
		float2(-1.0, -1.0), float2(1.0, -1.0), float2(-1.0, 1.0),
		float2(1.0, -1.0), float2(1.0, 1.0), float2(-1.0, 1.0)
	};

	const float2 uvs[6] = {
		float2(0.0, 0.0), float2(1.0, 0.0), float2(0.0, 1.0),
		float2(1.0, 0.0), float2(1.0, 1.0), float2(0.0, 1.0)
	};

	output.pos = float4(positions[vIdx], 0.0, 1.0);
	output.uv = uvs[vIdx];
	return output;
}

[[vk::binding(0, 0)]]
Sampler2D canvasTex;

[[vk::push_constant]]
MouseData mouse;

float distToSegment(float2 p, float2 a, float2 b) {
	float2 pa = p - a, ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h);
}

[shader("fragment")]
float4 fragmentPaint(VertexOut input) :
		SV_Target {
	float4 currentColor = canvasTex.Sample(input.uv);
	const float radius = 0.005;
	const float aspect = 1920.0 / 1080.0;

	float2 p = float2(input.uv.x * aspect, input.uv.y);
	float2 a = float2(mouse.lastX * aspect, mouse.lastY);
	float2 b = float2(mouse.x * aspect, mouse.y);

	float dist = distToSegment(p, a, b);

	float edgeWidth = 0.0005;
	float alpha = 1.0 - smoothstep(radius - edgeWidth, radius, dist);

	if (alpha > 0.0) {
		if (mouse.erase) {
			return lerp(currentColor, float4(1.0, 1.0, 1.0, 1.0), alpha);
		} else {
			return lerp(currentColor, float4(mouse.color, 1.0), alpha);
		}
	}

	return currentColor;
}
[shader("fragment")]
float4 fragmentDisplay(VertexOut input) :
		SV_Target {
	return canvasTex.Sample(input.uv);
}
