#include "framework.h"

// vertex shader in GLSL: It is a Raw string (C++11) since it contains new line characters
const char * const vertexSource = R"(
	#version 330				// Shader 3.3
	precision highp float;		// normal floats, makes no difference on desktop computers

	uniform mat4 MVP;			// uniform variable, the Model-View-Projection transformation matrix
	layout(location = 0) in vec2 vp;	// Varying input: vp = vertex position is expected in attrib array 0

	void main() {
        vec4 normal = vec4(vp.x, vp.y, 0, 1) * MVP;     // transform vp from modeling space to normalized device space
        vec4 hyper = vec4(normal.x, normal.y, 0, sqrt(pow(normal.x,2) + pow(normal.y, 2) + 1));
        gl_Position = vec4(hyper.x/hyper.w, hyper.y/hyper.w, 0, 1);
	}
)";

// fragment shader in GLSL
const char * const fragmentSource = R"(
	#version 330			// Shader 3.3
	precision highp float;	// normal floats, makes no difference on desktop computers

	uniform vec3 color;		// uniform variable, the color of the primitive
	out vec4 outColor;		// computed color of the current pixel

	void main() {
		outColor = vec4(color, 1);	// computed color is the color of the primitive
	}
)";

GPUProgram gpuProgram; // vertex and fragment shaders

const int nv = 100;
const float etoltes= 1.24f * pow(10,-8);
const int maxsuly = 50;
const int maxtoltes = 30;
const float idolepes = 0.01f;
const float kozeg = 1.0f;
const float alak = 0.45f;

class Camera2D {
    vec2 wCenter;
public:
    Camera2D(): wCenter(0,0){}

    mat4 V() {
        return TranslateMatrix(-wCenter);
    }

    void Pan(vec2 t){
        wCenter = wCenter +t;
    }
};

Camera2D camera;

class Atom{
    unsigned int vao, vbo;   // vertex buffer object
public:
    int tomeg;
    float toltes;
    vec2 pos, F, v;


    Atom(int t, float toltes):tomeg(t),toltes(toltes),F(0,0){}

    void create(){
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        vec2 vertices[nv];
        for (int i= 0; i< nv;i++){
            float fi = i* 2* M_PI / nv;
            vertices[i] = vec2(cosf(fi), sinf(fi));
        }
        glBufferData(GL_ARRAY_BUFFER, 	// Copy to GPU target
                     sizeof(vec2) * nv,  // # bytes
                     vertices,	      	// address
                     GL_STATIC_DRAW);	// we do not change later


        glEnableVertexAttribArray(0);  // AttribArray 0
        glVertexAttribPointer(0,       // vbo -> AttribArray 0
                              2, GL_FLOAT, GL_FALSE, // two floats/attrib, not fixed-point
                              0, NULL); 		     // stride, offset: tightly packed
    }

    mat4 M(mat4 m){
        mat4 Mscale={0.05f, 0, 0, 0,
                    0, 0.05f, 0, 0,
                    0, 0,  1, 0,
                    0, 0,  0, 1};
        mat4 Mtranslate{ 1, 0, 0, 0,
                         0, 1, 0, 0,
                         0, 0, 1, 0,
                         pos.x+m[3][0], pos.y+m[3][1], 0, 1 };
        return Mscale * Mtranslate;
    }

    void draw(mat4 m){
        vec3 c= ((toltes>0) ? vec3(1,0,0) : vec3(0,0,1))* (fabs(toltes/maxtoltes)*0.75f+0.25f);

        // Set color
        int location = glGetUniformLocation(gpuProgram.getId(), "color");
        glUniform3f(location, c.x, c.y, c.z);

        mat4 MVPtransf =  (M(m)) * camera.V();
        gpuProgram.setUniform(MVPtransf, "MVP");

        glBindVertexArray(vao);  // Draw call
        glDrawArrays(GL_TRIANGLE_FAN, 0 , nv );
    }
};

class LineStrip {
    unsigned int		vao, vbo;	// vertex array object, vertex buffer object
    std::vector<float>  vertexData; // interleaved data of coordinates and colors
public:
    vec2			    wTranslate; // translation
    std::vector<vec2>   controlPoints; // interleaved data of coordinates and colors
    float phi=0;

    void create() {
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        // Enable the vertex attribute arrays
        glEnableVertexAttribArray(0);  // attribute array 0
        glEnableVertexAttribArray(1);  // attribute array 1
        // Map attribute array 0 to the vertex data of the interleaved vbo
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(0));
        // Map attribute array 1 to the color data of the interleaved vbo
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), reinterpret_cast<void*>(2 * sizeof(float)));
    }


    mat4 M(){
        mat4 Mrotate={cosf(phi), sinf(phi), 0, 0,
                      -sinf(phi), cosf(phi), 0, 0,
                      0,        0,        1, 0,
                      0,        0,        0, 1};
        mat4 Mtranslate{ 1,            0,            0, 0,
                    0,            1,            0, 0,
                    0,            0,            1, 0,
                    wTranslate.x, wTranslate.y, 0, 1};
        return Mrotate*Mtranslate;
    }

    void addPoint(float cX, float cY) {
        // input pipeline
        vec4 mVertex = vec4(cX, cY, 0, 1);
        controlPoints.push_back(vec2(mVertex.x, mVertex.y));
        // fill interleaved data
        vertexData.push_back(mVertex.x);
        vertexData.push_back(mVertex.y);
        vertexData.push_back(1); // red
        vertexData.push_back(1); // green
        vertexData.push_back(0); // blue
        // copy data to the GPU
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertexData.size() * sizeof(float), &vertexData[0], GL_DYNAMIC_DRAW);
    }

    void addTranslation(vec2 wT) { wTranslate = wTranslate + wT; }

    void draw() {
        if (vertexData.size() > 0) {
            int location = glGetUniformLocation(gpuProgram.getId(), "color");
            glUniform3f(location, 1.0f, 1.0f, 1.0f); // 3 floats

            mat4 MVPTransform = M() * camera.V();
            gpuProgram.setUniform(MVPTransform, "MVP");
            glBindVertexArray(vao);
            glDrawArrays(GL_LINE_STRIP, 0, vertexData.size() / 5);
        }
    }
};

struct Molekula{
    std::vector<Atom> atomok;
    float ossztomeg, phi, forgatonyomatek;
    vec2 sulypont, F, v;
    std::vector<LineStrip> lines;
    bool kapcsolatok[8][8];

    void createLines(){
        for (int i = 0; i < atomok.size() - 1; i++) {
            for (int j = i + 1; j < atomok.size(); j++) {
                if (kapcsolatok[i][j]) {
                    LineStrip l;
                    l.create();
                    l.addPoint(atomok[i].pos.x,atomok[i].pos.y);
                    float xkulonbseg = atomok[j].pos.x - atomok[i].pos.x;
                    float ykulonbseg = atomok[j].pos.y - atomok[i].pos.y;
                    const float felosztas=25;
                    for (int k = 1; k < felosztas; k++) {
                        vec2 koztes= vec2(xkulonbseg*(float)k/felosztas+atomok[i].pos.x,ykulonbseg*(float)k/felosztas+atomok[i].pos.y);
                        l.addPoint(koztes.x,koztes.y);
                    }
                    l.addPoint(atomok[j].pos.x,atomok[j].pos.y);
                    lines.push_back(l);
                }
            }
        }
    }

    Molekula():F(0,0),v(0,0),sulypont(0,0), forgatonyomatek(0),phi(0),ossztomeg(0){
        for (int i=0;i<8;i++){
            for (int j=0;j<8;j++){
                kapcsolatok[i][j]=false;
            }
        }
        int db = rand() % 7 + 2;
        float ossztolt = 0;
        for (int i=0;i<db;i++){
            int tomeg = rand() % maxsuly +1;
            float toltes;
            if (i+1<db) {
                toltes = (rand() % maxtoltes + 1) * ((rand() % 2 == 0) ? 1 : -1);
                ossztolt += toltes;
            }else{
                toltes = ossztolt * -1;
            }
            Atom a(tomeg,toltes);
            a.pos= vec2(rand() % 101 * 0.01f * ((rand() % 2 == 0) ? 1 : -1),rand() % 101 * 0.01f * ((rand() % 2 == 0) ? 1 : -1));
            ossztomeg+= a.tomeg;
            sulypont = sulypont + vec2(a.pos.x*a.tomeg,a.pos.y*a.tomeg);
            if (i>0){
                int kapcs = rand() % i;
                kapcsolatok[kapcs][i] = true;
                kapcsolatok[i][kapcs] = true;
            }
            atomok.push_back(a);
        }
        sulypont = sulypont/ ossztomeg;
        for (int i=0;i<atomok.size();i++){
            atomok[i].pos.x = atomok[i].pos.x-sulypont.x;
            atomok[i].pos.y = atomok[i].pos.y-sulypont.y;
        }
        createLines();
    }

    void create(){
        for (int i;i<atomok.size();i++) {
            atomok[i].create();
        }
    }

    mat4 m(){
        mat4 Mrotate={cosf(phi), sinf(phi), 0, 0,
                     -sinf(phi), cosf(phi), 0, 0,
                        0, 0, 1, 0,
                        0, 0, 0, 1 };
        mat4 Mtranslate{ 1, 0, 0, 0,
                         0, 1, 0, 0,
                         0, 0, 1, 0,
                         sulypont.x, sulypont.y, 0, 1 };
        return Mrotate * Mtranslate;
    }

    void draw() {
        for (int i = 0; i < lines.size(); i++) {
            lines[i].addTranslation(sulypont-lines[i].wTranslate);
            lines[i].phi=phi;
            lines[i].draw();
        }
        for (int i = 0; i < atomok.size(); i++) {
            atomok[i].draw(m());
        }
    }

    void animate() {
        sulypont = sulypont + v;
        for (int i = 0; i < atomok.size(); i++) {
            float szog =atan2f(atomok[i].pos.y,atomok[i].pos.x);
            atomok[i].pos = length(atomok[i].pos) * vec2(cosf(szog+phi), sinf(szog+phi));
        }
        lines.clear();
        createLines();
    }
};


std::vector<Molekula> molekulak;

void calculateForces(){
    for (int i=0;i<molekulak.size();i++){
        molekulak[i].F = vec2(0,0);
        molekulak[i].phi = 0;
        float theta = 0;
        for (int j=0;j<molekulak[i].atomok.size();j++){
            molekulak[i].atomok[j].F = vec2(0,0);
            vec2 realpos1 = molekulak[i].atomok[j].pos+molekulak[i].sulypont;
            molekulak[i].atomok[j].v = molekulak[i].v;
            for (int k=0;k<molekulak.size();k++){
                if (k!=i) {
                    for (int l = 0; l < molekulak[i].atomok.size(); l++) {
                        Atom a=molekulak[k].atomok[l];
                        vec2 realpos2 = a.pos+molekulak[l].sulypont;
                        vec2 tmpF(realpos2.x-realpos1.x, realpos2.y-realpos1.y);
                        tmpF = tmpF * (pow(etoltes,2)*a.toltes*molekulak[i].atomok[j].toltes/ (2.0f*M_PI*8.854187817f*pow(10,-12)*pow(length(tmpF),2)));
                        molekulak[i].atomok[j].F = molekulak[i].atomok[j].F + tmpF;
                    }
                }
            }
            molekulak[i].atomok[j].F = molekulak[i].atomok[j].F + (-0.5f) * vec2(pow(molekulak[i].atomok[j].v.x,2),pow(molekulak[i].atomok[j].v.y,2)) * kozeg * alak;
            molekulak[i].F = vec2(molekulak[i].F.x+molekulak[i].atomok[j].F.x,molekulak[i].F.y+molekulak[i].atomok[j].F.y);
            molekulak[i].forgatonyomatek = molekulak[i].forgatonyomatek + length(molekulak[i].atomok[j].pos * molekulak[i].atomok[j].F);
            theta = theta + molekulak[i].atomok[j].tomeg * dot(molekulak[i].atomok[j].pos,molekulak[i].atomok[j].pos);
        }
        molekulak[i].v = molekulak[i].v + (molekulak[i].F / molekulak[i].ossztomeg)*idolepes;
        molekulak[i].phi = molekulak[i].phi + (molekulak[i].forgatonyomatek/theta)*idolepes;
    }
}

// Initialization, create an OpenGL context
void onInitialization() {
    srand(time(0));
	glViewport(0, 0, windowWidth, windowHeight);
    glLineWidth(1.0f);

	// create program for the GPU
	gpuProgram.create(vertexSource, fragmentSource, "outColor");
}

// Window has become invalid: Redraw
void onDisplay() {
	glClearColor(0, 0, 0, 0);     // background color
	glClear(GL_COLOR_BUFFER_BIT); // clear frame buffer


    for (int i=0;i<molekulak.size();i++){
        molekulak[i].draw();
    }

    glutSwapBuffers(); // exchange buffers for double buffering
}

// Key of ASCII code pressed
void onKeyboard(unsigned char key, int pX, int pY) {
	switch (key) {
        case ' ':
            for (int i=0; i<2; i++){
                Molekula m;
                m.create();
                molekulak.push_back(m);
            }
            break;
        case 's':
            camera.Pan(vec2(-0.1f,0));
            break;
        case 'd':
            camera.Pan(vec2(0.1f,0));
            break;
        case 'x':
            camera.Pan(vec2(0,-0.1f));
            break;
        case 'e':
            camera.Pan(vec2(0,0.1f));
            break;
    }
    glutPostRedisplay(); // if d, invalidate display, i.e. redraw
}

// Key of ASCII code released
void onKeyboardUp(unsigned char key, int pX, int pY) {
}

// Move mouse with key pressed
void onMouseMotion(int pX, int pY) {	// pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
}

// Mouse click event
void onMouse(int button, int state, int pX, int pY) { // pX, pY are the pixel coordinates of the cursor in the coordinate system of the operation system
}


long lasttime=0;
// Idle event indicating that some time elapsed: do animation here
void onIdle() {
	long time = glutGet(GLUT_ELAPSED_TIME); // elapsed time since the start of the program
    long dtime = time -lasttime;
    for (int k=0;k<dtime/10;k++) {
        calculateForces();
        for (int i = 0; i < molekulak.size(); i++) {
            molekulak[i].animate();
        }
    }
    lasttime=time;
    glutPostRedisplay();
}
