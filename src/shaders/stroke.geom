#version 410 core

#define PI 3.14159265359
#define PI_2 1.57079632679
#define MASK_EPS 0.001
#define CAP_RES 14
#define MAX_VTX 74              // at least 12 + CAP_RES * 4 + 2
#define ST PI_2/CAP_RES
#define ST2 PI/CAP_RES
#define UP vec2(0.0, 1.0)
#define RIGHT vec2(1.0, 0.0)

layout (lines_adjacency) in;
layout (triangle_strip, max_vertices = MAX_VTX) out;

in VTX {
    float pressure;
    float visibility;
    int id;
    vec4 color;
} in_vtx[];

out float iVisibility;
out vec4 iColor;

uniform vec2 winSize;
uniform float zoom;
uniform float thetaEpsilon;
uniform float strokeWeight;
uniform float time;
uniform int capIdx[2];
uniform int groupId;
uniform bool ignoreMask;
uniform bool sticker;
uniform bool displayVisibility;
uniform sampler2D maskStrength;
uniform int maskMode;

// stroke body
void make_quad(int i0, int i1, vec2 n0, vec2 n1, float w0, float w1) {
    gl_Position = gl_in[i0].gl_Position + vec4(n0*w0/winSize, 0.0, 0.0); 
    iVisibility = in_vtx[i0].visibility;
    iColor = in_vtx[i0].color;
    EmitVertex();
    gl_Position = gl_in[i0].gl_Position - vec4(n0*w0/winSize, 0.0, 0.0); 
    EmitVertex();
    gl_Position = gl_in[i1].gl_Position + vec4(n1*w1/winSize, 0.0, 0.0); 
    iVisibility = in_vtx[i1].visibility;
    iColor = in_vtx[i1].color;
    EmitVertex();
    gl_Position = gl_in[i1].gl_Position - vec4(n1*w1/winSize, 0.0, 0.0); 
    EmitVertex();
    EndPrimitive();
}

// round cap
// params: gl_in index, width, rotation (rad) 
void make_semi_circle_strip(int i, float w, float o) {
    vec2 d;
    mat2 rot = mat2(cos(o), sin(o), -sin(o), cos(o));
    iVisibility = in_vtx[i].visibility;
    iColor = in_vtx[i].color;
    for (float theta = PI_2; theta > 0; theta -= ST) {
        d = vec2(cos(theta), sin(theta))*w;
        gl_Position = gl_in[i].gl_Position + vec4(rot*d/winSize, 0.0, 0.0);
        EmitVertex();
        gl_Position = gl_in[i].gl_Position + vec4(rot*vec2(d.x, -d.y)/winSize, 0.0, 0.0);
        EmitVertex();
    }
    gl_Position = gl_in[i].gl_Position + vec4(rot*(RIGHT*w)/winSize, 0.0, 0.0);
    EmitVertex();
    EndPrimitive();
}

// round joint
void make_arc_strip(int i, float w, float theta_start, float theta_end, float o) {
    float tot = theta_end - theta_start;
    if (tot > PI) tot -= 2 * PI;
    else if (tot <= -PI) tot += 2 * PI;
    tot = abs(tot);
    float tot_2 = tot * 0.5;
    float s = tot < ST ? tot_2 : ST;
    vec2 d;
    mat2 rot = mat2(cos(o), sin(o), -sin(o), cos(o));
    gl_Position = gl_in[i].gl_Position;
    iVisibility = in_vtx[i].visibility;
    iColor = in_vtx[i].color;
    EmitVertex();
    for (float theta = tot_2; theta > 0; theta -= s) {
        d = vec2(cos(theta), sin(theta))*w;
        gl_Position = gl_in[i].gl_Position + vec4(rot*d/winSize, 0.0, 0.0);
        EmitVertex();
        gl_Position = gl_in[i].gl_Position + vec4(rot*vec2(d.x, -d.y)/winSize, 0.0, 0.0);
        EmitVertex();
    }
    gl_Position = gl_in[i].gl_Position + vec4(rot*(RIGHT*w)/winSize, 0.0, 0.0);
    EmitVertex();
    EndPrimitive();
}

// bevel joint
void make_tri(int i, float w, vec2 n0, vec2 n1) {
    gl_Position = gl_in[i].gl_Position;
    iVisibility = in_vtx[i].visibility;
    iColor = in_vtx[i].color;
    EmitVertex();
    gl_Position = gl_in[i].gl_Position + vec4(n0*w/winSize, 0.0, 0.0);
    EmitVertex();
    gl_Position = gl_in[i].gl_Position + vec4(n1*w/winSize, 0.0, 0.0);;
    EmitVertex();
    EndPrimitive();
}

void main() {
    //       tp            t0            t1
    // vp - - - - - v0------------v1 - - - - - vn    

    // vp and vn are here for adjacency info
    // position are converted to screen space for computation then reconverted to NDC before emitting vertices
    vec2 vp = gl_in[0].gl_Position.xy * winSize;
    vec2 v0 = gl_in[1].gl_Position.xy * winSize;
    vec2 v1 = gl_in[2].gl_Position.xy * winSize;
    vec2 vn = gl_in[3].gl_Position.xy * winSize;

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

    // mask value at v0 and v1
    float mask0 = 1.0 - texture(maskStrength, (gl_in[1].gl_Position.xy+vec2(1.0))*0.5).r;
    float mask1 = 1.0 - texture(maskStrength, (gl_in[2].gl_Position.xy+vec2(1.0))*0.5).r;

    if (!ignoreMask && maskMode == 0) {
        if (!sticker && (mask0 < MASK_EPS || mask1 < MASK_EPS)) {
            return;
        }
        if (sticker && (mask0 >= 1.0 - MASK_EPS || mask1 >= 1.0 - MASK_EPS)) {
            return;
        }
        // mask0 = mask0 * 0.9 + 0.1;
        // mask1 = mask1 * 0.9 + 0.1;
    } 
    
    // else {
    //     mask0 = 1.0;
    //     mask1 = 1.0;       
    // }

    // stroke width at v0 and v1
    float vis = float((in_vtx[1].visibility  >= -1.0 && (in_vtx[1].visibility >= 0.0 ? time >= in_vtx[1].visibility : -time > in_vtx[1].visibility)) || (in_vtx[2].visibility >= -1.0 && (in_vtx[2].visibility >= 0.0 ? time >= in_vtx[2].visibility : -time > in_vtx[2].visibility)));
    float w0 = strokeWeight * in_vtx[1].pressure * zoom * vis/* * mask0*/;
    float w1 = strokeWeight * in_vtx[2].pressure * zoom * vis/* * mask1*/;

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
    if (in_vtx[1].id != capIdx[0]) {
        if (b0 > 0.2) {
            float offset = (dot(tp, n0) > 0) ? 0.0 : -PI; 
            make_arc_strip(1, w0, atan(np.y, np.x), atan(n0.y, n0.x), atan(m0.y, m0.x)+offset);
        } else if (b0 > thetaEpsilon) {
            float sgn = sign(dot(tp, n0)); // ! unhandled edge case when sign returns 0 but it probably can't happen because of the else if test
            make_tri(1, w0, sgn * np, sgn * n0);
        } else {
            side0 = m0;
        }
    }

    // check if the right side is a miter joint
    if (b1 <= thetaEpsilon) {
        side1 = m1;
    }

    // main stroke body
    make_quad(1, 2, side0, side1, w0, w1);

    // start cap
    if (in_vtx[1].id == capIdx[0]) {
        float theta_offset = sign(dot(n0, RIGHT)) * acos(dot(n0, -UP));
        make_semi_circle_strip(1, w0, theta_offset);
    } 

    // end cap
    if (in_vtx[2].id == capIdx[1]) {
        float theta_offset = sign(dot(n0, -RIGHT)) * acos(dot(n0, UP));
        make_semi_circle_strip(2, w1, theta_offset);
    }
} 