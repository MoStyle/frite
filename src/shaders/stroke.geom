#version 410 core

#define PI 3.14159265359
#define PI_2 1.57079632679
#define CAP_RES 14
#define MAX_VTX 74              // at least 12 + CAP_RES * 4 + 2
#define ST PI_2/CAP_RES
#define ST2 PI/CAP_RES
#define UP vec2(0.0, 1.0)
#define RIGHT vec2(1.0, 0.0)

layout (lines_adjacency) in;
layout (triangle_strip, max_vertices = MAX_VTX) out;

uniform vec2 win_size;
uniform float zoom;
uniform float theta_epsilon;
uniform float stroke_weight;
uniform int cap_idx[2];

in VTX {
    float pressure;
    float type;
    int id;
} in_vtx[];

// stroke body
void make_quad(int i0, int i1, vec2 n0, vec2 n1, float w0, float w1) {
    gl_Position = gl_in[i0].gl_Position + vec4(n0*w0/win_size, 0.0, 0.0); 
    EmitVertex();
    gl_Position = gl_in[i0].gl_Position - vec4(n0*w0/win_size, 0.0, 0.0); 
    EmitVertex();
    gl_Position = gl_in[i1].gl_Position + vec4(n1*w1/win_size, 0.0, 0.0); 
    EmitVertex();
    gl_Position = gl_in[i1].gl_Position - vec4(n1*w1/win_size, 0.0, 0.0); 
    EmitVertex();
    EndPrimitive();
}

// round cap
// params: gl_in index, width, rotation (rad) 
void make_semi_circle_strip(int i, float w, float o) {
    vec2 d;
    mat2 rot = mat2(cos(o), sin(o), -sin(o), cos(o));
    for (float theta = PI_2; theta > 0; theta -= ST) {
        d = vec2(cos(theta), sin(theta))*w;
        gl_Position = gl_in[i].gl_Position + vec4(rot*d/win_size, 0.0, 0.0);
        EmitVertex();
        gl_Position = gl_in[i].gl_Position + vec4(rot*vec2(d.x, -d.y)/win_size, 0.0, 0.0);
        EmitVertex();
    }
    gl_Position = gl_in[i].gl_Position + vec4(rot*(RIGHT*w)/win_size, 0.0, 0.0);
    EmitVertex();
    EndPrimitive();
}

// round joint
void make_arc_strip(int i, float w, float theta_start, float theta_end, float o) {
    float tot = abs(theta_end - theta_start);
    float tot_2 = tot * 0.5;
    float s = tot < ST ? tot_2 : ST;
    vec2 d;
    mat2 rot = mat2(cos(o), sin(o), -sin(o), cos(o));
    gl_Position = gl_in[i].gl_Position;
    EmitVertex();
    for (float theta = tot_2; theta > 0; theta -= s) {
        d = vec2(cos(theta), sin(theta))*w;
        gl_Position = gl_in[i].gl_Position + vec4(rot*d/win_size, 0.0, 0.0);
        EmitVertex();
        gl_Position = gl_in[i].gl_Position + vec4(rot*vec2(d.x, -d.y)/win_size, 0.0, 0.0);
        EmitVertex();
    }
    gl_Position = gl_in[i].gl_Position + vec4(rot*(RIGHT*w)/win_size, 0.0, 0.0);
    EmitVertex();
    EndPrimitive();
}

// bevel joint
void make_tri(int i, float w, vec2 n0, vec2 n1) {
    gl_Position = gl_in[i].gl_Position;
    EmitVertex();
    gl_Position = gl_in[i].gl_Position + vec4(n0*w/win_size, 0.0, 0.0);
    EmitVertex();
    gl_Position = gl_in[i].gl_Position + vec4(n1*w/win_size, 0.0, 0.0);;
    EmitVertex();
    EndPrimitive();
}

void main() {
    //       tp            t0            t1
    // vp - - - - - v0------------v1 - - - - - vn    

    // vp and vn are here for adjacency info
    // position are converted to screen space for computation then reconverted to NDC before emitting vertices
    vec2 vp = gl_in[0].gl_Position.xy * win_size;
    vec2 v0 = gl_in[1].gl_Position.xy * win_size;
    vec2 v1 = gl_in[2].gl_Position.xy * win_size;
    vec2 vn = gl_in[3].gl_Position.xy * win_size;

    // tangents of the 3 segments
    vec2 tp = normalize(v0 - vp);
    vec2 t0 = normalize(v1 - v0);
    vec2 t1 = normalize(vn - v1);

    // normals of the 3 segments
    vec2 np = vec2(-tp.y, tp.x);
    vec2 n0 = vec2(-t0.y, t0.x);
    vec2 n1 = vec2(-t1.y, t1.x);

    // miter vector at v0 and v1
    vec2 m0 = normalize(np + n0);
    vec2 m1 = normalize(n1 + n0);

    // stroke width at v0 and v1
    float w0 = stroke_weight * in_vtx[1].pressure * zoom;
    float w1 = stroke_weight * in_vtx[2].pressure * zoom;

    // bend angle
    float b0 = acos(dot(tp, t0));
    float b1 = acos(dot(t0, t1));

    // sides
    vec2 side0 = n0;
    vec2 side1 = n0;

    // construct a joint if the angle discontinuity is greater than a threshold
    // large discontinuity: round joint
    // medium discontinuity: bevel joint
    // small discontinuity: miter joint
    if (b0 > 0.2) {
        float offset = (dot(tp, n0) > 0) ? 0.0 : -PI; 
        make_arc_strip(1, w0, atan(np.y, np.x), atan(n0.y, n0.x), atan(m0.y, m0.x)+offset);
    } else if (b0 > theta_epsilon) {
        float sgn = sign(dot(tp, n0)); // ! unhandled edge case when sign returns 0 but it probably can't happen because of the else if test
        make_tri(1, w0, sgn * np, sgn * n0);
    } else {
        side0 = m0;
    }

    // check if the right side is a miter joint
    if (b1 <= theta_epsilon) {
        side1 = m1;
    }

    // main stroke body
    make_quad(1, 2, side0, side1, w0, w1);

    // start cap
    if (in_vtx[0].id == cap_idx[0]) {
        float theta_offset = sign(dot(np, RIGHT)) * acos(dot(np, -UP));
        make_quad(0, 1, np, np, stroke_weight * in_vtx[0].pressure * zoom, w0);
        make_semi_circle_strip(0, stroke_weight * in_vtx[0].pressure * zoom, theta_offset);
    } 

    // end cap
    if (in_vtx[2].id == cap_idx[1]) {
        float theta_offset = sign(dot(n0, -RIGHT)) * acos(dot(n0, UP));
        make_semi_circle_strip(2, w1, theta_offset);
    }
} 