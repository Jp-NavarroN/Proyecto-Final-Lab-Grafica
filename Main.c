#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cglm/cglm.h>

#define LADDERS 9
#define FLOORS 7
#define BARRELS 20
#define TIMER 24
#define NUM_BONUS 4

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct t_coord { int x, y, z, a; } t_coord;
typedef struct t_coord_kf { float x, y, z, a; } t_coord_kf;

t_coord screen, angle, start;
t_coord mario, pauline, dk, oil;
t_coord ladder[LADDERS];
t_coord barrel[BARRELS];
t_coord flooring[FLOORS][2];

t_coord_kf currentBeamKF;

int jumping, onJump, walking, climbing, onClimb, bros, barrelBlock;
int points, pointsLast;
double timeLeft;
int gameState, cameraMode, deathFloor, moving;
float aspect;

int pointLightsActive = 0;

unsigned int shaderProgram;
unsigned int cubeVAO, cubeVBO;
unsigned int objVAO = 0, objVBO = 0;
int objVertexCount = 0;

unsigned int beamTexture = 0;

typedef struct { float time, yOffset, rotation; } Keyframe;
Keyframe animKeys[5] = {
    {0.0f,0.0f,0.0f},{0.5f,20.0f,90.0f},{1.0f,0.0f,180.0f},{1.5f,-20.0f,270.0f},{2.0f,0.0f,360.0f}
};
int numKeys = 5;
float currentAnimTime = 0.0f, currentYOffset = 0.0f, currentRotation = 0.0f;

int itemCollected[NUM_BONUS] = { 0,0,0,0 };
t_coord bonusItems[NUM_BONUS] = { {400,-585,0,0},{-400,-285,0,0},{400,15,0,0},{-350,315,0,0} };

mat4 currentMatrix;
mat4 matrixStack[128];
int stackPointer = 0;

void pushMatrix() { if (stackPointer < 128) { glm_mat4_copy(currentMatrix, matrixStack[stackPointer]);stackPointer++; } }
void popMatrix() { if (stackPointer > 0) { stackPointer--;glm_mat4_copy(matrixStack[stackPointer], currentMatrix); } }
void translate(float x, float y, float z) { glm_translate(currentMatrix, (vec3) { x, y, z }); }
void rotate(float a, float x, float y, float z) { glm_rotate(currentMatrix, glm_rad(a), (vec3) { x, y, z }); }
void scale(float x, float y, float z) { glm_scale(currentMatrix, (vec3) { x, y, z }); }
void setColor(float r, float g, float b) { glUniform3f(glGetUniformLocation(shaderProgram, "objectColor"), r, g, b); }

void drawSolidCube(float size) {
    mat4 temp; glm_mat4_copy(currentMatrix, temp);
    glm_scale(temp, (vec3) { size, size, size });
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, (float*)temp);
    glBindVertexArray(cubeVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

#define K 0
#define M 1
#define B 2
#define H 3

#define TEX_W 32
#define TEX_H 16

void createBeamTexture() {
    static const unsigned char pal[4][3] = {
        {  0,   0,   0},
        {204,   0, 119},
        {100,   0,  58},
        {255, 100, 200},
    };

    unsigned char pixels[TEX_H][TEX_W][3];

    for (int r = 0; r < TEX_H; r++)
        for (int c = 0; c < TEX_W; c++) {
            pixels[r][c][0] = pal[K][0];
            pixels[r][c][1] = pal[K][1];
            pixels[r][c][2] = pal[K][2];
        }

    for (int c = 0; c < TEX_W; c++) {
        pixels[0][c][0] = pal[B][0]; pixels[0][c][1] = pal[B][1]; pixels[0][c][2] = pal[B][2];
        pixels[1][c][0] = pal[M][0]; pixels[1][c][1] = pal[M][1]; pixels[1][c][2] = pal[M][2];
        pixels[2][c][0] = pal[H][0]; pixels[2][c][1] = pal[H][1]; pixels[2][c][2] = pal[H][2];
        pixels[13][c][0] = pal[M][0]; pixels[13][c][1] = pal[M][1]; pixels[13][c][2] = pal[M][2];
        pixels[14][c][0] = pal[B][0]; pixels[14][c][1] = pal[B][1]; pixels[14][c][2] = pal[B][2];
        pixels[15][c][0] = pal[B][0]; pixels[15][c][1] = pal[B][1]; pixels[15][c][2] = pal[B][2];
    }

    int body_top = 3;
    int body_bot = 12;
    int body_h = body_bot - body_top + 1;
    int cell_w = 10;
    int half = cell_w / 2;

    for (int c = 0; c < TEX_W; c++) {
        int cell = c / cell_w;
        int cx = c % cell_w;

        for (int r = body_top; r <= body_bot; r++) {
            int ry = r - body_top;
            float slope = (float)half / (float)(body_h - 1);
            int diag_left = (int)(ry * slope + 0.5f);
            int diag_right = cell_w - 1 - diag_left;

            int on_left = (cx == diag_left || cx == diag_left + 1);
            int on_right = (cx == diag_right || cx == diag_right - 1);

            int cx2 = (c + half) % cell_w;
            int diag_left2 = (int)((body_h - 1 - ry) * slope + 0.5f);
            int diag_right2 = cell_w - 1 - diag_left2;
            int on_left2 = (cx2 == diag_left2 || cx2 == diag_left2 + 1);
            int on_right2 = (cx2 == diag_right2 || cx2 == diag_right2 - 1);

            if (on_left || on_right) {
                pixels[r][c][0] = pal[M][0]; pixels[r][c][1] = pal[M][1]; pixels[r][c][2] = pal[M][2];
                if (ry == 0 || ry == 1) {
                    pixels[r][c][0] = pal[H][0]; pixels[r][c][1] = pal[H][1]; pixels[r][c][2] = pal[H][2];
                }
            }
            else if (on_left2 || on_right2) {
                pixels[r][c][0] = pal[M][0]; pixels[r][c][1] = pal[M][1]; pixels[r][c][2] = pal[M][2];
                if (ry == body_h - 1 || ry == body_h - 2) {
                    pixels[r][c][0] = pal[H][0]; pixels[r][c][1] = pal[H][1]; pixels[r][c][2] = pal[H][2];
                }
            }
        }
    }

    for (int cell = 0; cell < TEX_W / cell_w; cell++) {
        int cpeak = cell * cell_w + half - 1;
        if (cpeak >= 0 && cpeak < TEX_W) {
            pixels[body_top][cpeak][0] = pal[M][0]; pixels[body_top][cpeak][1] = pal[M][1]; pixels[body_top][cpeak][2] = pal[M][2];
        }
        int cbase = cell * cell_w;
        if (cbase < TEX_W) {
            pixels[body_bot][cbase][0] = pal[M][0]; pixels[body_bot][cbase][1] = pal[M][1]; pixels[body_bot][cbase][2] = pal[M][2];
        }
    }

    glGenTextures(1, &beamTexture);
    glBindTexture(GL_TEXTURE_2D, beamTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_W, TEX_H, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
}

#define BEAM_TILING 1.0f

void drawFloor(float ancho, float grosor, float prof) {
    pushMatrix();
    scale(ancho, grosor, prof);
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 1);
    glUniform1f(glGetUniformLocation(shaderProgram, "tiling"), BEAM_TILING);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, beamTexture);
    glUniform1i(glGetUniformLocation(shaderProgram, "texture1"), 0);
    setColor(1.0f, 1.0f, 1.0f);
    drawSolidCube(1.0f);
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
    popMatrix();
}

int loadOBJ(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) { char fb[256];sprintf(fb, "%s.obj", path);file = fopen(fb, "r");if (!file)return 0; }
    vec3* tv = malloc(20000 * sizeof(vec3));vec2* tu = malloc(20000 * sizeof(vec2));vec3* tn = malloc(20000 * sizeof(vec3));
    if (!tv || !tu || !tn) { free(tv);free(tu);free(tn);fclose(file);return 0; }
    int vc = 1, vtc = 1, vnc = 1;
    float* vd = malloc(300000 * 8 * sizeof(float));int di = 0;
    if (!vd) { free(tv);free(tu);free(tn);fclose(file);return 0; }
    char lh[128];
    while (fscanf(file, "%s", lh) != EOF) {
        if (!strcmp(lh, "v")) { fscanf(file, "%f %f %f\n", &tv[vc][0], &tv[vc][1], &tv[vc][2]);vc++; }
        else if (!strcmp(lh, "vt")) { fscanf(file, "%f %f\n", &tu[vtc][0], &tu[vtc][1]);vtc++; }
        else if (!strcmp(lh, "vn")) { fscanf(file, "%f %f %f\n", &tn[vnc][0], &tn[vnc][1], &tn[vnc][2]);vnc++; }
        else if (!strcmp(lh, "f")) {
            unsigned int vi[3], ui[3], ni[3];
            if (fscanf(file, "%d/%d/%d %d/%d/%d %d/%d/%d\n", &vi[0], &ui[0], &ni[0], &vi[1], &ui[1], &ni[1], &vi[2], &ui[2], &ni[2]) == 9)
                for (int i = 0;i < 3;i++) {
                    vd[di++] = tv[vi[i]][0];vd[di++] = tv[vi[i]][1];vd[di++] = tv[vi[i]][2];
                    vd[di++] = tn[ni[i]][0];vd[di++] = tn[ni[i]][1];vd[di++] = tn[ni[i]][2];
                    vd[di++] = tu[ui[i]][0];vd[di++] = tu[ui[i]][1];
                }
            else { char c = fgetc(file);while (c != '\n' && c != EOF)c = fgetc(file); }
        }
    }
    fclose(file);
    if (di > 0) {
        glGenVertexArrays(1, &objVAO);glGenBuffers(1, &objVBO);glBindVertexArray(objVAO);
        glBindBuffer(GL_ARRAY_BUFFER, objVBO);glBufferData(GL_ARRAY_BUFFER, di * sizeof(float), vd, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));glEnableVertexAttribArray(2);
        objVertexCount = di / 8;
    }
    free(tv);free(tu);free(tn);free(vd);
    return 1;
}

int getDepth(int y) {
    if (y >= -900 && y < -600)return 0;   if (y >= -600 && y < -300)return -100;
    if (y >= -300 && y < 0)   return -200; if (y >= 0 && y < 300) return -300;
    if (y >= 300 && y < 600) return -400; if (y >= 600 && y < 900) return -500;
    if (y >= 900 && y < 1200)return -600; return 100;
}

void drawMario() {
    if (bros == 0)setColor(248 / 255.0f, 56 / 255.0f, 0.0f);else setColor(0.0f, 1.0f, 0.0f);
    pushMatrix();drawSolidCube(100);popMatrix();
    setColor(1.0f, 173 / 255.0f, 82 / 255.0f);
    pushMatrix();translate(-20, 15, 50);drawSolidCube(8);popMatrix();
    pushMatrix();translate(20, 15, 50);drawSolidCube(8);popMatrix();
    pushMatrix();translate(0, 80, 0);drawSolidCube(70);popMatrix();
    setColor(0.0f, 0.0f, 173 / 255.0f);
    pushMatrix();translate(-15, 90, 35);drawSolidCube(10);popMatrix();
    pushMatrix();translate(15, 90, 35);drawSolidCube(10);popMatrix();
    pushMatrix();translate(0, 70, 35);scale(3, 1, 2);drawSolidCube(10);popMatrix();
    pushMatrix();translate(0, 65, 35);scale(2.5f, 1, 1.5f);drawSolidCube(10);popMatrix();
    pushMatrix();translate(0, 100, -6);scale(1, 0.6f, 1);drawSolidCube(76);popMatrix();
    if (bros == 0)setColor(248 / 255.0f, 56 / 255.0f, 0);else setColor(0, 1, 0);
    pushMatrix();translate(0, 128, 5);scale(0.9f, 0.25f, 1);drawSolidCube(60);popMatrix();
    pushMatrix();translate(0, 135, 5);scale(0.9f, 0.25f, 1);drawSolidCube(50);popMatrix();
    pushMatrix();
    if (walking && !jumping)rotate(mario.a * 2.0f, 0, 1, 0);
    else if (jumping)rotate(-30, 0, 0, 1);else if (onClimb)rotate(-45, 0, 0, 1);
    setColor(0, 0, 173 / 255.0f);pushMatrix();translate(-70, 15, 0);scale(1.2f, 1, 1);drawSolidCube(35);popMatrix();
    setColor(1, 173 / 255.0f, 82 / 255.0f);pushMatrix();translate(-95, 15, 0);scale(0.5f, 1, 1);drawSolidCube(30);popMatrix();
    popMatrix();
    pushMatrix();
    if (walking && !jumping)rotate(mario.a * 2.0f, 0, 1, 0);
    else if (jumping)rotate(30, 0, 0, 1);else if (onClimb)rotate(45, 0, 0, 1);
    setColor(0, 0, 173 / 255.0f);pushMatrix();translate(70, 15, 0);scale(1.2f, 1, 1);drawSolidCube(35);popMatrix();
    setColor(1, 173 / 255.0f, 82 / 255.0f);pushMatrix();translate(95, 15, 0);scale(0.5f, 1, 1);drawSolidCube(30);popMatrix();
    popMatrix();
    setColor(0, 0, 173 / 255.0f);
    pushMatrix();if ((walking || (climbing && onClimb)) && !jumping)translate(-30, -55 + mario.a, 0);else translate(-30, -65, 0);scale(0.8f, 1.2f, 1);drawSolidCube(40);popMatrix();
    pushMatrix();if ((walking || (climbing && onClimb)) && !jumping)translate(30, -55 - mario.a, 0);else translate(30, -65, 0);scale(0.8f, 1.2f, 1);drawSolidCube(40);popMatrix();
}

void drawPauline() {
    pushMatrix();translate(0, 30 + pauline.a, 0);
    setColor(1, 43 / 255.0f, 170 / 255.0f);pushMatrix();scale(1, 1, 0.75f);drawSolidCube(100);popMatrix();
    setColor(1, 1, 1);pushMatrix();translate(0, -40, 0);scale(1.2f, 0.1f, 0.9f);drawSolidCube(100);popMatrix();
    setColor(0, 0, 173 / 255.0f);pushMatrix();translate(0, -5, 0);scale(1.05f, 0.1f, 0.8f);drawSolidCube(100);popMatrix();
    setColor(1, 1, 1);pushMatrix();translate(0, 80, 0);drawSolidCube(70);popMatrix();
    setColor(1, 85 / 255.0f, 0);
    pushMatrix();translate(-15, 90, 35);drawSolidCube(10);popMatrix();
    pushMatrix();translate(15, 90, 35);drawSolidCube(10);popMatrix();
    pushMatrix();translate(0, 70, 35);scale(1.5f, 1, 1);drawSolidCube(10);popMatrix();
    pushMatrix();translate(0, 100, -5);scale(1.2f, 0.8f, 0.9f);drawSolidCube(75);popMatrix();
    pushMatrix();translate(0, 100, -5);scale(1.1f, 1.1f, 0.9f);drawSolidCube(75);popMatrix();
    pushMatrix();translate(0, 80, -50);scale(1, 0.95f, 1.4f);drawSolidCube(50);popMatrix();
    pushMatrix();if (gameState == 0)rotate(-30, 0, 0, 1);else rotate(-pauline.a / 2.0f, 0, 0, 1);
    setColor(1, 43 / 255.0f, 170 / 255.0f);pushMatrix();translate(-65, 15, 0);scale(1.2f, 1, 1);drawSolidCube(35);popMatrix();
    setColor(1, 1, 1);pushMatrix();translate(-95, 15, 0);scale(0.5f, 1, 1);drawSolidCube(30);popMatrix();popMatrix();
    pushMatrix();if (gameState == 0)rotate(30, 0, 0, 1);else rotate(pauline.a / 2.0f, 0, 0, 1);
    setColor(1, 43 / 255.0f, 170 / 255.0f);pushMatrix();translate(65, 15, 0);scale(1.2f, 1, 1);drawSolidCube(35);popMatrix();
    setColor(1, 1, 1);pushMatrix();translate(95, 15, 0);scale(0.5f, 1, 1);drawSolidCube(30);popMatrix();popMatrix();
    setColor(0, 0, 173 / 255.0f);
    pushMatrix();translate(-20, -65, 0);scale(0.8f, 1.2f, 1);drawSolidCube(40);popMatrix();
    pushMatrix();translate(20, -65, 0);scale(0.8f, 1.2f, 1);drawSolidCube(40);popMatrix();
    popMatrix();
}

void drawDK() {
    pushMatrix();rotate(dk.a, 0, 1, 0);
    setColor(170 / 255.0f, 0, 0);
    pushMatrix();translate(0, -8, -10);scale(1.1f, 1.3f, 1.0f);drawSolidCube(105);popMatrix();
    pushMatrix();translate(0, -8, -2);scale(1.1f, 1.1f, 0.9f);drawSolidCube(105);popMatrix();
    pushMatrix();translate(0, 18, -2);scale(1.15f, 0.6f, 0.9f);drawSolidCube(105);popMatrix();
    setColor(1, 170 / 255.0f, 85 / 255.0f);pushMatrix();translate(0, -8, -2);scale(1, 1.2f, 1);drawSolidCube(105);popMatrix();
    setColor(170 / 255.0f, 0, 0);
    pushMatrix();translate(-25, 18, 50);drawSolidCube(8);popMatrix();
    pushMatrix();translate(25, 18, 50);drawSolidCube(8);popMatrix();
    setColor(248 / 255.0f, 56 / 255.0f, 0);pushMatrix();translate(0, 6, 50);scale(2, 12, 1);drawSolidCube(8);popMatrix();
    pushMatrix();translate(0, 5, 0);rotate(dk.a, 0, 1, 0);
    setColor(1, 173 / 255.0f, 82 / 255.0f);pushMatrix();translate(0, 88, 0);drawSolidCube(70);popMatrix();
    setColor(170 / 255.0f, 0, 0);
    pushMatrix();translate(0, 90, -5);drawSolidCube(76);popMatrix();
    pushMatrix();translate(0, 90, -5);scale(1.1f, 0.9f, 1);drawSolidCube(76);popMatrix();
    pushMatrix();translate(0, 90, -5);scale(0.9f, 1.1f, 1);drawSolidCube(76);popMatrix();
    setColor(1, 1, 1);pushMatrix();translate(-15, 98, 30);drawSolidCube(12);popMatrix();
    setColor(170 / 255.0f, 0, 0);pushMatrix();translate(-12, 95, 35);drawSolidCube(7);popMatrix();
    setColor(1, 1, 1);pushMatrix();translate(15, 98, 30);drawSolidCube(12);popMatrix();
    setColor(170 / 255.0f, 0, 0);pushMatrix();translate(12, 95, 35);drawSolidCube(7);popMatrix();
    if (gameState != 0) { setColor(1, 1, 1);pushMatrix();translate(0, 75 + pauline.a / 10.0f, 35);scale(4.2f, 0.8f, 1.1f);drawSolidCube(10);popMatrix(); }
    setColor(0, 0, 0);pushMatrix();translate(0, 70, 31);
    if (gameState != 0)scale(4.2f, 0.8f, 0.9f);else scale(4.2f, 1.6f, 0.9f);
    drawSolidCube(10);popMatrix();
    if (gameState != 0) { setColor(1, 1, 1);pushMatrix();translate(0, 65 - pauline.a / 10.0f, 35);scale(4.2f, 0.8f, 1.1f);drawSolidCube(10);popMatrix(); }
    popMatrix();
    pushMatrix();rotate(dk.a / 2.0f, 0, 0, 1);
    setColor(170 / 255.0f, 0, 0);pushMatrix();translate(-75, 15, 0);scale(1.5f, 1, 1);drawSolidCube(35);popMatrix();
    setColor(1, 170 / 255.0f, 85 / 255.0f);pushMatrix();translate(-115, 15, 0);drawSolidCube(36);popMatrix();popMatrix();
    pushMatrix();rotate(dk.a / 2.0f, 0, 0, 1);
    setColor(170 / 255.0f, 0, 0);pushMatrix();translate(75, 15, 0);scale(1.5f, 1, 1);drawSolidCube(35);popMatrix();
    setColor(1, 170 / 255.0f, 85 / 255.0f);pushMatrix();translate(115, 15, 0);drawSolidCube(36);popMatrix();popMatrix();
    setColor(170 / 255.0f, 0, 0);
    pushMatrix();translate(-28, -70, 0);scale(1, 1.5f, 1);drawSolidCube(40);popMatrix();
    setColor(1, 170 / 255.0f, 85 / 255.0f);pushMatrix();translate(-28, -104, 4);scale(1, 0.2f, 1.2f);drawSolidCube(40);popMatrix();
    setColor(170 / 255.0f, 0, 0);pushMatrix();translate(28, -70, 0);scale(1, 1.5f, 1);drawSolidCube(40);popMatrix();
    setColor(1, 170 / 255.0f, 85 / 255.0f);pushMatrix();translate(28, -104, 4);scale(1, 0.2f, 1.2f);drawSolidCube(40);popMatrix();
    popMatrix();
}

void drawLadder() {
    setColor(0, 1, 1);
    pushMatrix();translate(-30, -1, -38);scale(0.1f, 2.08f, 0.1f);drawSolidCube(150);popMatrix();
    pushMatrix();translate(30, -1, -38);scale(0.1f, 2.08f, 0.1f);drawSolidCube(150);popMatrix();
    for (int i = 0;i < 7;i++) { pushMatrix();translate(0, 120 + (-40 * i), -38);scale(0.5f, 0.1f, 0.1f);drawSolidCube(150);popMatrix(); }
}

void drawOil() {
    pushMatrix();translate(-90, 10, 0);scale(-1, 1, 1);rotate(90, 0, 0, 1);
    for (int i = 0;i < 9;i++) {
        pushMatrix();rotate(i * 40.0f, 0, 1, 0);
        setColor(0, 0, 1);pushMatrix();scale(0.85f, 0.5f, 0.75f);drawSolidCube(150);popMatrix();
        pushMatrix();scale(0.75f, 0.75f, 0.75f);drawSolidCube(150);popMatrix();
        setColor(0, 1, 1);pushMatrix();scale(0.9f, 0.2f, 0.8f);drawSolidCube(150);popMatrix();
        setColor(1, 1, 1);pushMatrix();translate(0, -30, 0);scale(0.9f, 0.05f, 0.8f);drawSolidCube(150);popMatrix();
        setColor(0, 0, 50 / 255.0f);pushMatrix();translate(0, 55, 0);scale(0.7f, 0.025f, 0.7f);drawSolidCube(150);popMatrix();
        popMatrix();
    }
    if (gameState == 1) {
        pushMatrix();scale(0.75f, 1, 0.75f);rotate(dk.a, 0, 1, 0);
        for (int i = 0;i < 5;i++) {
            setColor(1, 213 / 255.0f, 0);pushMatrix();translate(-50 + rand() % 100, 60 + rand() % 100, 0);scale(1, 1, 10);drawSolidCube(5 + rand() % 15);popMatrix();
            setColor(1, 170 / 255.0f, 0);pushMatrix();translate(-50 + rand() % 100, 60 + rand() % 90, 0);scale(1, 1, 8);drawSolidCube(5 + rand() % 15);popMatrix();
            setColor(1, 0, 0);pushMatrix();translate(-50 + rand() % 100, 60 + rand() % 80, 0);scale(1, 1, 6);drawSolidCube(5 + rand() % 15);popMatrix();
            setColor(1, 1, 1);pushMatrix();translate(-50 + rand() % 100, 60 + rand() % 70, 0);scale(1, 1, 4);drawSolidCube(5 + rand() % 15);popMatrix();
        }
        popMatrix();
    }
    popMatrix();
}

void drawBarrel() {
    for (int i = 0;i < 9;i++) {
        pushMatrix();translate(0, 0, 20);rotate(40.0f * i, 0, 0, 1);scale(1, 1, 0.75f);
        setColor(1, 85 / 255.0f, 0);pushMatrix();scale(0.5f, 0.5f, 0.75f);drawSolidCube(150);popMatrix();
        setColor(1, 170 / 255.0f, 85 / 255.0f);pushMatrix();scale(0.6f, 0.5f, 0.7f);drawSolidCube(150);popMatrix();
        pushMatrix();scale(0.5f, 0.6f, 0.7f);drawSolidCube(150);popMatrix();
        setColor(0, 0, 1);
        pushMatrix();translate(0, 0, -35);rotate(90, 0, 1, 0);scale(0.05f, 0.45f, 0.65f);drawSolidCube(150);popMatrix();
        pushMatrix();translate(0, 0, 35);rotate(90, 0, 1, 0);scale(0.05f, 0.45f, 0.65f);drawSolidCube(150);popMatrix();
        pushMatrix();translate(0, 0, -35);rotate(90, 0, 1, 0);scale(0.05f, 0.65f, 0.45f);drawSolidCube(150);popMatrix();
        pushMatrix();translate(0, 0, 35);rotate(90, 0, 1, 0);scale(0.05f, 0.65f, 0.45f);drawSolidCube(150);popMatrix();
        pushMatrix();translate(0, 35, 0);scale(0.075f, 0.075f, 0.8f);drawSolidCube(150);popMatrix();
        popMatrix();
    }
}

const int pixelFont[10][5][3] = {
    {{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}},{{0,1,0},{1,1,0},{0,1,0},{0,1,0},{1,1,1}},
    {{1,1,1},{0,0,1},{1,1,1},{1,0,0},{1,1,1}},{{1,1,1},{0,0,1},{1,1,1},{0,0,1},{1,1,1}},
    {{1,0,1},{1,0,1},{1,1,1},{0,0,1},{0,0,1}},{{1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1}},
    {{1,1,1},{1,0,0},{1,1,1},{1,0,1},{1,1,1}},{{1,1,1},{0,0,1},{0,0,1},{0,0,1},{0,0,1}},
    {{1,1,1},{1,0,1},{1,1,1},{1,0,1},{1,1,1}},{{1,1,1},{1,0,1},{1,1,1},{0,0,1},{1,1,1}}
};
const int letterFont[26][5][3] = {
    {{1,1,1},{1,0,1},{1,1,1},{1,0,1},{1,0,1}},{{1,1,0},{1,0,1},{1,1,0},{1,0,1},{1,1,0}},
    {{1,1,1},{1,0,0},{1,0,0},{1,0,0},{1,1,1}},{{1,1,0},{1,0,1},{1,0,1},{1,0,1},{1,1,0}},
    {{1,1,1},{1,0,0},{1,1,0},{1,0,0},{1,1,1}},{{1,1,1},{1,0,0},{1,1,0},{1,0,0},{1,0,0}},
    {{1,1,1},{1,0,0},{1,0,1},{1,0,1},{1,1,1}},{{1,0,1},{1,0,1},{1,1,1},{1,0,1},{1,0,1}},
    {{1,1,1},{0,1,0},{0,1,0},{0,1,0},{1,1,1}},{{0,0,1},{0,0,1},{0,0,1},{1,0,1},{1,1,1}},
    {{1,0,1},{1,0,1},{1,1,0},{1,0,1},{1,0,1}},{{1,0,0},{1,0,0},{1,0,0},{1,0,0},{1,1,1}},
    {{1,0,1},{1,1,1},{1,0,1},{1,0,1},{1,0,1}},{{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,0,1}},
    {{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}},{{1,1,1},{1,0,1},{1,1,1},{1,0,0},{1,0,0}},
    {{1,1,1},{1,0,1},{1,0,1},{1,1,1},{0,0,1}},{{1,1,1},{1,0,1},{1,1,0},{1,0,1},{1,0,1}},
    {{1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1}},{{1,1,1},{0,1,0},{0,1,0},{0,1,0},{0,1,0}},
    {{1,0,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}},{{1,0,1},{1,0,1},{1,0,1},{1,0,1},{0,1,0}},
    {{1,0,1},{1,0,1},{1,0,1},{1,1,1},{1,0,1}},{{1,0,1},{1,0,1},{0,1,0},{1,0,1},{1,0,1}},
    {{1,0,1},{1,0,1},{0,1,0},{0,1,0},{0,1,0}},{{1,1,1},{0,0,1},{0,1,0},{1,0,0},{1,1,1}}
};
void drawHUDChar(char c) {
    int fm[5][3] = { 0 };
    if (c >= '0' && c <= '9') { int d = c - '0';for (int r = 0;r < 5;r++)for (int col = 0;col < 3;col++)fm[r][col] = pixelFont[d][r][col]; }
    else if (c >= 'A' && c <= 'Z') { int d = c - 'A';for (int r = 0;r < 5;r++)for (int col = 0;col < 3;col++)fm[r][col] = letterFont[d][r][col]; }
    else return;
    for (int r = 0;r < 5;r++)for (int col = 0;col < 3;col++)if (fm[r][col]) { pushMatrix();translate(col * 22, -r * 22, 0);drawSolidCube(18);popMatrix(); }
}
void drawHUDString(const char* s) { for (int i = 0;s[i];i++) { pushMatrix();translate(i * 90, 0, 0);drawHUDChar(s[i]);popMatrix(); } }
void drawHUD() {
    char sc[10], tc[10];
    sprintf(sc, "%05d", points);sprintf(tc, "%04d", (int)timeLeft);
    setColor(1, 1, 1);
    pushMatrix();translate(100, 950, -600);drawHUDString("TIME");popMatrix();
    pushMatrix();translate(100, 820, -600);drawHUDString(tc);popMatrix();
    pushMatrix();translate(520, 950, -600);drawHUDString("SCORE");popMatrix();
    pushMatrix();translate(520, 820, -600);drawHUDString(sc);popMatrix();
}

void drawScene() {
    glm_mat4_identity(currentMatrix);
    drawHUD();
    glUniform1i(glGetUniformLocation(shaderProgram, "pointLightsActive"), pointLightsActive);
    vec3 ptLights[6] = {
        {0.0f, (float)(flooring[0][0].y + flooring[0][1].y) * 0.5f, (float)getDepth(-900) + 60.0f},
        {0.0f, (float)(flooring[1][0].y + flooring[1][1].y) * 0.5f, (float)getDepth(-600) + 60.0f},
        {0.0f, (float)(flooring[2][0].y + flooring[2][1].y) * 0.5f, (float)getDepth(-300) + 60.0f},
        {0.0f, (float)(flooring[3][0].y + flooring[3][1].y) * 0.5f, (float)getDepth(0) + 60.0f},
        {0.0f, (float)(flooring[4][0].y + flooring[4][1].y) * 0.5f, (float)getDepth(300) + 60.0f},
        {0.0f, (float)(flooring[5][0].y + flooring[5][1].y) * 0.5f, (float)getDepth(600) + 60.0f}
    };
    for (int i = 0;i < 6;i++) {
        char un[64];sprintf(un, "pointLightPositions[%d]", i);
        glUniform3fv(glGetUniformLocation(shaderProgram, un), 1, ptLights[i]);
    }
    if (pointLightsActive) {
        for (int i = 0;i < 6;i++) {
            pushMatrix();translate(ptLights[i][0], ptLights[i][1], ptLights[i][2]);
            scale(0.5f, 0.5f, 0.5f);setColor(1.0f, 1.0f, 0.8f);drawSolidCube(40);popMatrix();
        }
    }
    if (objVertexCount > 0) {
        for (int j = 0;j < NUM_BONUS;j++) {
            if (!itemCollected[j]) {
                pushMatrix();
                translate(bonusItems[j].x, bonusItems[j].y + currentYOffset, getDepth(bonusItems[j].y) - 20);
                rotate(currentRotation, 0, 1, 0);scale(150, 150, 150);setColor(1.0f, 0.8f, 0.1f);
                glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, (float*)currentMatrix);
                glBindVertexArray(objVAO);glDrawArrays(GL_TRIANGLES, 0, objVertexCount);
                popMatrix();
            }
        }
    }
    if (cameraMode != 2) {
        pushMatrix();translate(mario.x, mario.y, getDepth(mario.y));
        if (gameState == -1)rotate(90.0f * (deathFloor + 1), 1, 0, 0);
        if (walking)rotate(90, 0, walking, 0);
        if (onClimb)rotate(180, 0, 1, 0);
        scale(0.5f, 0.5f, 0.5f);drawMario();popMatrix();
    }
    pushMatrix();translate(dk.x, dk.y, getDepth(600) - 50);
    if (gameState == 0) { translate(0, 0, -150);rotate(deathFloor * 10.0f, 1, 1, 1); }
    scale(1.5f, 1.5f, 1.5f);drawDK();popMatrix();
    if (gameState == 1) {
        float torreX = (float)dk.x - 300.0f;
        float torreY = (float)flooring[5][0].y;
        float torreZ = (float)getDepth(600) - 500.0f;
        float bs = 1.0f, diam = 100.0f, alto = 100.0f;
        for (int b = 0;b < 3;b++) { pushMatrix();translate(torreX + (b - 1) * diam, torreY, torreZ);scale(bs, bs, bs);drawBarrel();popMatrix(); }
        for (int b = 0;b < 2;b++) { pushMatrix();translate(torreX + (b - 0.5f) * diam, torreY + alto, torreZ);scale(bs, bs, bs);drawBarrel();popMatrix(); }
        pushMatrix();translate(torreX, torreY + alto * 2.0f, torreZ);scale(bs, bs, bs);drawBarrel();popMatrix();
    }
    pushMatrix();translate(pauline.x, pauline.y, getDepth(900));scale(0.5f, 0.5f, 0.5f);drawPauline();popMatrix();
    for (int i = 0;i < LADDERS;i++) { pushMatrix();translate(ladder[i].x, ladder[i].y, getDepth(ladder[i].y - 100));drawLadder();popMatrix(); }
    for (int i = 0;i < BARRELS;i++) {
        if (barrel[i].z != 0) {
            pushMatrix();translate(barrel[i].x, barrel[i].y + 5, getDepth(barrel[i].y) - 10);
            if (abs(barrel[i].z) == 1)rotate(barrel[i].a, 0, 0, 1);
            else if (abs(barrel[i].z) == 2) { translate(-16, 0, 0);rotate(90, 0, 1, 0);rotate(abs(barrel[i].a), 0, 0, 1); }
            drawBarrel();popMatrix();
        }
    }
    pushMatrix();translate(oil.x, oil.y, getDepth(-900));drawOil();popMatrix();

#define BEAM_OFFSET 70.0f

    for (int i = 0;i < FLOORS - 1;i++) {
        int midY = (flooring[i][0].y + flooring[i][1].y) / 2;
        float cx = (flooring[i][0].x + flooring[i][1].x) * 0.5f;
        float ancho = (float)(flooring[i][1].x - flooring[i][0].x) + 92.5f;
        pushMatrix();
        translate(cx, (float)flooring[i][0].y - BEAM_OFFSET, (float)getDepth(midY));
        drawFloor(ancho, 28.0f, 80.0f);
        popMatrix();
    }
    {
        int i = FLOORS - 1;
        int midY = (flooring[i][0].y + flooring[i][1].y) / 2;
        float cx = (flooring[i][0].x + flooring[i][1].x) * 0.5f;
        float ancho = (float)(flooring[i][1].x - flooring[i][0].x);
        pushMatrix();
        translate(cx + currentBeamKF.x, (float)flooring[i][0].y - BEAM_OFFSET + currentBeamKF.y, (float)getDepth(midY) + currentBeamKF.z);
        rotate(currentBeamKF.a, 0, 1, 0);
        drawFloor(ancho, 28.0f, 80.0f);
        popMatrix();
    }
    if (gameState != 0) {
        pushMatrix();translate(-750.0f, 530.0f, (float)getDepth(900) - 10);drawFloor(800.0f, 28.0f, 80.0f);popMatrix();
        pushMatrix();translate(-325.0f, 850.0f, (float)getDepth(600) - 10);drawFloor(150.0f, 28.0f, 80.0f);popMatrix();
    }
}

t_coord_kf beamKFs[2] = { {0,0,0,-5},{0,0,0,5} };
void interpolateKF(float t, t_coord_kf s, t_coord_kf e, t_coord_kf* r) {
    r->x = s.x + t * (e.x - s.x);r->y = s.y + t * (e.y - s.y);r->z = s.z + t * (e.z - s.z);r->a = s.a + t * (e.a - s.a);
}

void reset() {
    gameState = 1;cameraMode = 0;aspect = 1.0f;deathFloor = 0;
    points = 0;pointsLast = -1;timeLeft = 300;
    angle.x = 0;angle.y = 0;angle.z = 0;start.x = 0;start.y = 0;start.z = 0;
    mario.x = -700;mario.y = -900;mario.z = 1;mario.a = 0;
    dk.x = -620;dk.y = 718;dk.z = 1;dk.a = 0;
    pauline.x = -325;pauline.y = 900;pauline.z = 1;pauline.a = 0;
    oil.x = -850;oil.y = -888;
    for (int i = 0;i < NUM_BONUS;i++)itemCollected[i] = 0;
    ladder[0].x = 575;ladder[0].y = -800;ladder[1].x = -150;ladder[1].y = -500;
    ladder[2].x = -575;ladder[2].y = -500;ladder[3].x = 0;ladder[3].y = -200;
    ladder[4].x = 575;ladder[4].y = -200;ladder[5].x = -300;ladder[5].y = 100;
    ladder[6].x = -575;ladder[6].y = 100;ladder[7].x = 575;ladder[7].y = 400;ladder[8].x = -175;ladder[8].y = 700;
    for (int i = 0;i < BARRELS;i++) { barrel[i].x = -480;barrel[i].y = 600;barrel[i].z = 0;barrel[i].a = 0; }
    flooring[0][0].x = -850;flooring[0][0].y = -900;flooring[0][1].x = 950;flooring[0][1].y = -750;
    flooring[1][0].x = -950;flooring[1][0].y = -600;flooring[1][1].x = 850;flooring[1][1].y = -450;
    flooring[2][0].x = -650;flooring[2][0].y = -300;flooring[2][1].x = 950;flooring[2][1].y = -150;
    flooring[3][0].x = -950;flooring[3][0].y = 0;flooring[3][1].x = 800;flooring[3][1].y = 150;
    flooring[4][0].x = -650;flooring[4][0].y = 300;flooring[4][1].x = 950;flooring[4][1].y = 450;
    flooring[5][0].x = -350;flooring[5][0].y = 600;flooring[5][1].x = 650;flooring[5][1].y = 750;
    flooring[6][0].x = -225;flooring[6][0].y = 900;flooring[6][1].x = -130;flooring[6][1].y = 1050;
    bros = 0;barrelBlock = 0;onJump = 0;onClimb = 0;jumping = 0;walking = 0;climbing = 0;
}

int checkLadder(int x, int y, int range) { for (int i = 0;i < LADDERS;i++)if (ladder[i].x - range <= x && x <= ladder[i].x + range && ladder[i].y - 100 <= y && y <= ladder[i].y + 200)return 1;return 0; }
int checkFloor(int x, int y) { for (int i = 0;i < FLOORS;i++)if (flooring[i][0].x <= x && x <= flooring[i][1].x && flooring[i][0].y <= y && y <= flooring[i][1].y)return 1;return 0; }
int checkFall(int x, int y, int dir) { for (int i = 0;i < FLOORS;i++) { if (dir == 1 && flooring[i][1].x <= x && flooring[i][0].y <= y && y <= flooring[i][1].y)return 1;if (dir == -1 && flooring[i][0].x >= x && flooring[i][0].y <= y && y <= flooring[i][1].y)return 1; }return 0; }
int checkCollision(int x1, int y1, int x2, int y2, int rx, int ry) { return(x1 - rx <= x2 && x2 <= x1 + rx && y1 - ry <= y2 && y2 <= y1 + ry && getDepth(y1) == getDepth(y2)); }
int findFreeBarrel() { for (int i = 0;i < BARRELS;i++)if (!barrel[i].z)return i;return -1; }
void returnBarrel(int i) { barrel[i].x = -480;barrel[i].y = 600;barrel[i].z = 0;barrel[i].a = 0; }
void addPoints(int p) { if (points + p > 99999)points = 99999;else points += p; }
void pauseGame() { if (gameState == 2) { gameState = 1;walking = 0; } else if (gameState == 1)gameState = 2; }

void animate() {
    if (gameState == 1) {
        int i;timeLeft -= 0.075;
        if (timeLeft <= 0.0) { timeLeft = 0.0;gameState = -1; }
        currentAnimTime += 0.02f;
        if (currentAnimTime > animKeys[numKeys - 1].time)currentAnimTime = 0.0f;
        for (int k = 0;k < numKeys - 1;k++) {
            if (currentAnimTime >= animKeys[k].time && currentAnimTime <= animKeys[k + 1].time) {
                float t = (currentAnimTime - animKeys[k].time) / (animKeys[k + 1].time - animKeys[k].time);
                currentYOffset = animKeys[k].yOffset + t * (animKeys[k + 1].yOffset - animKeys[k].yOffset);
                currentRotation = animKeys[k].rotation + t * (animKeys[k + 1].rotation - animKeys[k].rotation);
                break;
            }
        }
        static float bat = 0.0f;static int bd = 1;
        bat += 0.005f * bd;if (bat >= 1.0f) { bat = 1.0f;bd = -1; }
        else if (bat <= 0.0f) { bat = 0.0f;bd = 1; }
        interpolateKF(bat, beamKFs[0], beamKFs[1], &currentBeamKF);
        for (int j = 0;j < NUM_BONUS;j++)
            if (!itemCollected[j] && checkCollision(bonusItems[j].x, bonusItems[j].y, mario.x, mario.y, 60, 100)) { itemCollected[j] = 1;addPoints(1000); }
        dk.a += dk.z;if (dk.a == dk.z * 30)dk.z = -dk.z;
        pauline.a += pauline.z * 3;if (pauline.a == pauline.z * 30)pauline.z = -pauline.z;
        mario.a += mario.z * 2;if (mario.a == mario.z * 16)mario.z = -mario.z;
        if (!barrelBlock) { if (rand() % 25 == 0) { i = findFreeBarrel();if (i >= 0) { barrel[i].z = 1;barrelBlock = 125; } } }
        else barrelBlock--;
        for (i = 0;i < BARRELS;i++) {
            if (barrel[i].z != 0) {
                if (barrel[i].a < 360)barrel[i].a += (barrel[i].z * -5);else barrel[i].a = 0;
                if (checkCollision(barrel[i].x, barrel[i].y, mario.x, mario.y, 70, 95)) {
                    mario.y -= abs(mario.y % 10);returnBarrel(i);
                    walking = 0;climbing = 0;onClimb = 0;jumping = 1;gameState = -1;
                }
                else if (checkCollision(barrel[i].x, barrel[i].y, mario.x, mario.y, 10, 200) && jumping && pointsLast != i) { addPoints(500);pointsLast = i; }
                else if (checkCollision(barrel[i].x, barrel[i].y, oil.x, oil.y, 35, 25)) { returnBarrel(i); }
                else if (checkCollision(pauline.x, pauline.y, mario.x, mario.y, 100, 250)) {
                    dk.x = 90;dk.y = 710;pauline.x = -315;pauline.y = 885;pauline.z = 1;pauline.a = 0;
                    walking = 0;climbing = 0;onClimb = 0;jumping = 1;gameState = 0;
                }
                if (abs(barrel[i].z) == 1) {
                    if (checkFall(barrel[i].x + (-barrel[i].z * 75), barrel[i].y, barrel[i].z) || (rand() % 5 == 0 && checkLadder(barrel[i].x, barrel[i].y - 150, 0))) {
                        if (barrel[i].z == 1)barrel[i].z = 2;else if (barrel[i].z == -1)barrel[i].z = -2;
                        barrel[i].a = 0;barrel[i].y -= 10;
                    }
                    else barrel[i].x += (barrel[i].z * 5);
                }
                else if (abs(barrel[i].z) == 2) {
                    if (barrel[i].y % 300 != 0)barrel[i].y -= 10;
                    else { if (barrel[i].z == 2)barrel[i].z = -1;else if (barrel[i].z == -2)barrel[i].z = 1;barrel[i].a = 0; }
                }
            }
        }
        if (jumping != 0) {
            if (jumping == 1) { if (mario.y < onJump + 90)mario.y += 10;else if (mario.y < onJump + 150)mario.y += 5;else jumping = -1; }
            else { if (mario.y > onJump)mario.y -= 5;else jumping = 0; }
        }
        if (walking && checkFloor(mario.x + (walking * 5), mario.y))mario.x += (walking * 5);
        if (climbing) { if (checkLadder(mario.x, mario.y + (climbing * 5), 20))mario.y += (climbing * 5);else onClimb = 0; }
    }
    else if (gameState == -1) {
        mario.y -= 10;if (mario.y % 300 == 0)deathFloor++;if (mario.y <= -1200)reset();
    }
    else if (gameState == 0) {
        if ((int)timeLeft > 0.0) { addPoints(100);timeLeft -= 1.0; }
        dk.y -= 10;deathFloor++;if (dk.y <= -2500)reset();
    }
}

void setCamera() {
    mat4 view, proj;glm_mat4_identity(view);glm_mat4_identity(proj);
    if (cameraMode == 2) {
        float ex = mario.x, ey = mario.y + 50, ez = getDepth(mario.y);
        float lx = ex, ly = ey, lz = ez + 100;
        if (walking == 1) { lx = ex + 100;lz = ez; }
        else if (walking == -1) { lx = ex - 100;lz = ez; }
        else if (onClimb) { ly = ey + 100;lz = ez - 100; }
        glm_lookat((vec3) { ex, ey, ez }, (vec3) { lx, ly, lz }, (vec3) { 0, 1, 0 }, view);
        glm_perspective(glm_rad(75.0f), aspect, 1.0f, 4000.0f, proj);
    }
    else if (cameraMode == 1) {
        glm_lookat((vec3) { mario.x, mario.y + 250, 1500 }, (vec3) { mario.x / 4.0f, mario.y / 4.0f, 0 }, (vec3) { 0, 1, 0 }, view);
        glm_perspective(glm_rad(75.0f), aspect, 1.0f, 4000.0f, proj);
    }
    else {
        glm_ortho(-screen.z * aspect, screen.z * aspect, -screen.z, screen.z, -screen.z, screen.z, proj);
        glm_lookat((vec3) { 0, 0, 1 }, (vec3) { 0, 0, 0 }, (vec3) { 0, 1, 0 }, view);
    }
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, (float*)view);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, (float*)proj);
    vec3 sd = { 0.0f,-0.1f,1.0f };
    if (walking == 1) { sd[0] = 0.8f;sd[1] = -0.1f;sd[2] = 0.5f; }
    else if (walking == -1) { sd[0] = -0.8f;sd[1] = -0.1f;sd[2] = 0.5f; }
    else if (onClimb) { sd[0] = 0.0f;sd[1] = 1.0f;sd[2] = 0.0f; }
    glm_vec3_normalize(sd);
    float mz = (float)getDepth(mario.y) + 60.0f;
    glUniform3f(glGetUniformLocation(shaderProgram, "spotLightPos"), (float)mario.x, (float)mario.y + 50.0f, mz);
    glUniform3fv(glGetUniformLocation(shaderProgram, "spotLightDir"), 1, sd);
    glUniform1f(glGetUniformLocation(shaderProgram, "spotLightCutoff"), cosf(glm_rad(55.0f)));
    glUniform1f(glGetUniformLocation(shaderProgram, "spotLightOuterCutoff"), cosf(glm_rad(70.0f)));
}

void initOpenGL() {
    const char* vs =
        "#version 330 core\n"
        "layout(location=0) in vec3 aPos;\n"
        "layout(location=1) in vec3 aNormal;\n"
        "layout(location=2) in vec2 aTexCoords;\n"
        "out vec3 FragPos; out vec3 Normal; out vec2 TexCoords;\n"
        "uniform mat4 model,view,projection;\n"
        "void main(){\n"
        "  FragPos=vec3(model*vec4(aPos,1.0));\n"
        "  Normal=mat3(transpose(inverse(model)))*aNormal;\n"
        "  TexCoords=aTexCoords;\n"
        "  gl_Position=projection*view*vec4(FragPos,1.0);\n"
        "}\n";
    const char* fs =
        "#version 330 core\n"
        "out vec4 FragColor;\n"
        "in vec3 FragPos; in vec3 Normal; in vec2 TexCoords;\n"
        "uniform vec3 objectColor;\n"
        "uniform sampler2D texture1;\n"
        "uniform int useTexture;\n"
        "uniform float tiling;\n"
        "uniform vec3 spotLightPos,spotLightDir;\n"
        "uniform float spotLightCutoff,spotLightOuterCutoff;\n"
        "uniform int pointLightsActive;\n"
        "uniform vec3 pointLightPositions[6];\n"
        "void main(){\n"
        "  vec3 color = (useTexture==1) ? texture(texture1,TexCoords*tiling).rgb : objectColor;\n"
        "  vec3 result = 0.25 * color;\n"
        "  vec3 norm = normalize(Normal);\n"
        "  vec3 toSpot = spotLightPos - FragPos;\n"
        "  vec3 sDir   = normalize(toSpot);\n"
        "  float theta = dot(sDir, normalize(-spotLightDir));\n"
        "  float eps   = spotLightCutoff - spotLightOuterCutoff;\n"
        "  float si    = clamp((theta - spotLightOuterCutoff)/eps, 0.0, 1.0);\n"
        "  float sd    = max(dot(norm, sDir), 0.0);\n"
        "  float sd2   = length(toSpot.xy);\n"
        "  float sa    = 1.0 / (1.0 + 0.000025 * sd2);\n"
        "  result += sd * color * 1.5 * si * sa;\n"
        "  if(pointLightsActive==1){\n"
        "    for(int i=0;i<6;i++){\n"
        "      vec3 toP  = pointLightPositions[i] - FragPos;\n"
        "      vec3 pd   = normalize(toP);\n"
        "      float pdd = abs(dot(norm, pd));\n"
        "      float pd2 = length(toP.xy);\n"
        "      float pa  = 1.0/(1.0+0.0012*pd2);\n"
        "      result += pdd * color * 0.6 * pa;\n"
        "    }\n"
        "  }\n"
        "  FragColor = vec4(result,1.0);\n"
        "}\n";
    unsigned int v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs, NULL);glCompileShader(v);
    unsigned int f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs, NULL);glCompileShader(f);
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, v);glAttachShader(shaderProgram, f);
    glLinkProgram(shaderProgram);
    glDeleteShader(v);glDeleteShader(f);

    // =========================================================================
    // ÚNICO CAMBIO: UVs del cubo corregidas para que la textura corra a lo
    // largo del eje X (horizontal) en las 6 caras.
    //
    // Criterio por cara:
    //   Cara frontal  (Z+): U = x+0.5, V = y+0.5  → textura horizontal ✓
    //   Cara trasera  (Z-): U = 0.5-x, V = y+0.5  → textura horizontal ✓
    //   Cara derecha  (X+): U = z+0.5, V = y+0.5  → profundidad mapeada ✓
    //   Cara izquierda(X-): U = 0.5-z, V = y+0.5  → profundidad mapeada ✓
    //   Cara superior (Y+): U = x+0.5, V = 0.5-z  → textura horizontal ✓
    //   Cara inferior (Y-): U = x+0.5, V = z+0.5  → textura horizontal ✓
    //
    // Formato por vértice: px py pz  nx ny nz  u v
    // =========================================================================
    float verts[] = {
        // Cara trasera (Z-)  normal 0,0,-1
        -0.5f,-0.5f,-0.5f, 0,0,-1,  1.0f,0.0f,
         0.5f,-0.5f,-0.5f, 0,0,-1,  0.0f,0.0f,
         0.5f, 0.5f,-0.5f, 0,0,-1,  0.0f,1.0f,
         0.5f, 0.5f,-0.5f, 0,0,-1,  0.0f,1.0f,
        -0.5f, 0.5f,-0.5f, 0,0,-1,  1.0f,1.0f,
        -0.5f,-0.5f,-0.5f, 0,0,-1,  1.0f,0.0f,
        // Cara frontal (Z+)  normal 0,0,1
        -0.5f,-0.5f, 0.5f, 0,0, 1,  0.0f,0.0f,
         0.5f,-0.5f, 0.5f, 0,0, 1,  1.0f,0.0f,
         0.5f, 0.5f, 0.5f, 0,0, 1,  1.0f,1.0f,
         0.5f, 0.5f, 0.5f, 0,0, 1,  1.0f,1.0f,
        -0.5f, 0.5f, 0.5f, 0,0, 1,  0.0f,1.0f,
        -0.5f,-0.5f, 0.5f, 0,0, 1,  0.0f,0.0f,
        // Cara izquierda (X-)  normal -1,0,0
        -0.5f, 0.5f, 0.5f,-1,0, 0,  1.0f,1.0f,
        -0.5f, 0.5f,-0.5f,-1,0, 0,  0.0f,1.0f,
        -0.5f,-0.5f,-0.5f,-1,0, 0,  0.0f,0.0f,
        -0.5f,-0.5f,-0.5f,-1,0, 0,  0.0f,0.0f,
        -0.5f,-0.5f, 0.5f,-1,0, 0,  1.0f,0.0f,
        -0.5f, 0.5f, 0.5f,-1,0, 0,  1.0f,1.0f,
        // Cara derecha (X+)  normal 1,0,0
         0.5f, 0.5f, 0.5f, 1,0, 0,  0.0f,1.0f,
         0.5f, 0.5f,-0.5f, 1,0, 0,  1.0f,1.0f,
         0.5f,-0.5f,-0.5f, 1,0, 0,  1.0f,0.0f,
         0.5f,-0.5f,-0.5f, 1,0, 0,  1.0f,0.0f,
         0.5f,-0.5f, 0.5f, 1,0, 0,  0.0f,0.0f,
         0.5f, 0.5f, 0.5f, 1,0, 0,  0.0f,1.0f,
         // Cara inferior (Y-)  normal 0,-1,0  — U corre en X, V en Z
         -0.5f,-0.5f,-0.5f, 0,-1,0,  0.0f,1.0f,
          0.5f,-0.5f,-0.5f, 0,-1,0,  1.0f,1.0f,
          0.5f,-0.5f, 0.5f, 0,-1,0,  1.0f,0.0f,
          0.5f,-0.5f, 0.5f, 0,-1,0,  1.0f,0.0f,
         -0.5f,-0.5f, 0.5f, 0,-1,0,  0.0f,0.0f,
         -0.5f,-0.5f,-0.5f, 0,-1,0,  0.0f,1.0f,
         // Cara superior (Y+)  normal 0,1,0   — U corre en X, V en Z
         -0.5f, 0.5f,-0.5f, 0, 1,0,  0.0f,1.0f,
          0.5f, 0.5f,-0.5f, 0, 1,0,  1.0f,1.0f,
          0.5f, 0.5f, 0.5f, 0, 1,0,  1.0f,0.0f,
          0.5f, 0.5f, 0.5f, 0, 1,0,  1.0f,0.0f,
         -0.5f, 0.5f, 0.5f, 0, 1,0,  0.0f,0.0f,
         -0.5f, 0.5f,-0.5f, 0, 1,0,  0.0f,1.0f
    };
    glGenVertexArrays(1, &cubeVAO);glGenBuffers(1, &cubeVBO);glBindVertexArray(cubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));glEnableVertexAttribArray(2);
    glEnable(GL_DEPTH_TEST);
    loadOBJ("modelo.obj");
    createBeamTexture();
    glUseProgram(shaderProgram);
    glUniform1i(glGetUniformLocation(shaderProgram, "useTexture"), 0);
    glUniform1f(glGetUniformLocation(shaderProgram, "tiling"), 1.0f);
}

void processInput(GLFWwindow* w) {
    if (gameState == 1) {
        walking = 0;climbing = 0;
        if (glfwGetKey(w, GLFW_KEY_LEFT) == GLFW_PRESS && !onClimb && checkFloor(mario.x - 5, mario.y))walking = -1;
        if (glfwGetKey(w, GLFW_KEY_RIGHT) == GLFW_PRESS && !onClimb && checkFloor(mario.x + 5, mario.y))walking = 1;
        if (glfwGetKey(w, GLFW_KEY_UP) == GLFW_PRESS) { if (!jumping && !walking && checkLadder(mario.x, mario.y + 5, 20)) { climbing = 1;onClimb = 1; } else onClimb = 0; }
        if (glfwGetKey(w, GLFW_KEY_DOWN) == GLFW_PRESS) { if (!jumping && !walking && checkLadder(mario.x, mario.y - 5, 20)) { climbing = -1;onClimb = 1; } else onClimb = 0; }
    }
}
void key_callback(GLFWwindow* w, int key, int sc, int action, int mods) {
    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_Q)glfwSetWindowShouldClose(w, 1);
        if (key == GLFW_KEY_R)reset();
        if (key == GLFW_KEY_C) { cameraMode = (cameraMode + 1) % 3;angle.x = 0;angle.y = 0; }
        if (key == GLFW_KEY_ENTER)pauseGame();
        if (key == GLFW_KEY_I)pointLightsActive = !pointLightsActive;
        if (gameState == 1) {
            if (key == GLFW_KEY_SPACE && !jumping && !climbing && !onClimb && mario.y < 900) { jumping = 1;onJump = mario.y; }
            if (key == GLFW_KEY_L)bros = (bros + 1) % 2;
            if (key == GLFW_KEY_W) { mario.x = -180;mario.y = 900; }
        }
    }
}
void framebuffer_size_callback(GLFWwindow* w, int width, int height) {
    if (!height)height = 1;glViewport(0, 0, width, height);
    aspect = (float)width / height;screen.x = width;screen.y = height;
}

int main() {
    screen.x = 1000;screen.y = 1000;screen.z = 1100;aspect = 1.0f;srand(time(NULL));
    if (!glfwInit())return -1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1000, 1000, "DK3D UNAM - OPENGL 3.3", NULL, NULL);
    if (!window) { glfwTerminate();return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK)return -1;
    initOpenGL();reset();
    double lt = glfwGetTime(), tt = TIMER / 1000.0, acc = 0.0;
    while (!glfwWindowShouldClose(window)) {
        double ct = glfwGetTime();acc += ct - lt;lt = ct;
        while (acc >= tt) { animate();acc -= tt; }
        processInput(window);
        glClearColor(0, 0, 0, 1);glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glUseProgram(shaderProgram);setCamera();drawScene();
        glfwSwapBuffers(window);glfwPollEvents();
    }
    glfwTerminate();return 0;
}
