/* Computacao Grafica - Unisinos
 * Iluminacao com tres fontes de luz (three-point lighting)
 *
 * Teclas:
 *   TAB          - troca o objeto selecionado
 *   R / T / S    - ativa modo girar / mover / escalar
 *   X, Y, Z      - define eixo de rotacao (modo girar)
 *   Setas + W/E  - movimenta o objeto (modo mover)
 *   = / -        - aumenta ou diminui o objeto (modo escalar)
 *   1 / 2 / 3    - liga ou desliga cada fonte de luz
 *   ESC          - encerra
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_EASY_FONT_IMPLEMENTATION
#include "stb_easy_font.h"

// --------------------------------------------------------------------------
// Prototipos
// --------------------------------------------------------------------------
void   key_callback(GLFWwindow* window, int key, int scancode, int action, int mod);
int    setupShader();
int    setupTextShader();
void   renderText(const string& str, float x, float y, float tam, glm::vec3 cor);
void   desenharHUD();
void   atualizarLuzes(GLuint prog, GLint posLoc[], GLint onLoc[]);
int    carregarOBJ(const string& caminho, int& qtdVerts, GLuint& texID,
                   float& ka, float& kd, float& ks, float& brilho);
GLuint carregarTextura(const string& caminho);

// --------------------------------------------------------------------------
// Dimensoes da janela
// --------------------------------------------------------------------------
const GLuint LARGURA = 1000, ALTURA = 1000;

// --------------------------------------------------------------------------
// Vertex Shader — transforma posicao e passa atributos para o fragment
// --------------------------------------------------------------------------
const GLchar* srcVertPrincipal = R"(
#version 330
layout (location = 0) in vec3 posicao;
layout (location = 1) in vec2 uvCoord;
layout (location = 2) in vec3 normal;
uniform mat4 model;
out vec2  coordUV;
out vec3  normalFrag;
out vec3  posicaoFrag;
void main()
{
    vec4 posM = model * vec4(posicao, 1.0);
    gl_Position  = posM;
    posicaoFrag  = vec3(posM);
    coordUV      = uvCoord;
    normalFrag   = normalize(mat3(transpose(inverse(model))) * normal);
}
)";

// Fragment Shader — calcula iluminacao de Phong para ate 3 luzes
const GLchar* srcFragPrincipal = R"(
#version 330
in vec2  coordUV;
in vec3  normalFrag;
in vec3  posicaoFrag;
uniform sampler2D mapa;
uniform vec3  posCamera;
uniform float ka;
uniform float kd;
uniform float ks;
uniform float q;
uniform vec3  lightPos[3];
uniform float lightInt[3];
uniform int   lightOn[3];
uniform float realce;
out vec4 fragColor;
void main()
{
    vec3 corBase   = vec3(texture(mapa, coordUV));
    vec3 norm      = normalize(normalFrag);
    vec3 visao     = normalize(posCamera - posicaoFrag);

    vec3 ambiental  = ka * corBase;
    vec3 difusa     = vec3(0.0);
    vec3 especular  = vec3(0.0);

    for (int i = 0; i < 3; i++)
    {
        if (lightOn[i] == 0) continue;

        vec3  direcaoLuz  = normalize(lightPos[i] - posicaoFrag);
        float distancia   = length(lightPos[i] - posicaoFrag);
        float atenuacao   = 1.0 / (1.0 + 0.5 * distancia + 1.0 * distancia * distancia);

        float fatorDifuso = max(dot(norm, direcaoLuz), 0.0);
        difusa   += kd * fatorDifuso * atenuacao * lightInt[i] * corBase;

        vec3  reflexo      = reflect(-direcaoLuz, norm);
        float fatorEspec   = pow(max(dot(reflexo, visao), 0.0), q);
        especular         += ks * fatorEspec * lightInt[i];
    }

    fragColor = vec4((ambiental + difusa + especular) * realce, 1.0);
}
)";

// Shaders do sistema de texto 2D (HUD)
const GLchar* srcVertTexto = R"(
#version 330
layout (location = 0) in vec2 posicao;
uniform mat4 projecao;
void main()
{
    gl_Position = projecao * vec4(posicao, 0.0, 1.0);
}
)";

const GLchar* srcFragTexto = R"(
#version 330
uniform vec3 corTexto;
out vec4 fragColor;
void main()
{
    fragColor = vec4(corTexto, 1.0);
}
)";

// --------------------------------------------------------------------------
// Estado da aplicacao
// --------------------------------------------------------------------------
enum class Modo { IDLE, SPIN, SLIDE, RESIZE };

struct Malha
{
    GLuint    id;
    int       qtdVerts;
    GLuint    textura;
    float     ka, kd, ks, brilho;
    glm::vec3 pos;
    glm::vec3 esc;
    glm::vec3 eixo;
    float     angulo;
    bool      girando;
    string    rotulo;

    Malha(GLuint id, int qtdVerts, GLuint tex,
          float ka, float kd, float ks, float brilho,
          glm::vec3 pos, string rotulo)
        : id(id), qtdVerts(qtdVerts), textura(tex),
          ka(ka), kd(kd), ks(ks), brilho(brilho),
          pos(pos), esc(glm::vec3(0.3f)),
          eixo(glm::vec3(0.0f, 1.0f, 0.0f)),
          angulo(0.0f), girando(false), rotulo(rotulo) {}
};

vector<Malha> cena;
int           atual        = 0;
Modo          modo         = Modo::IDLE;
bool          lucesAtivas[3] = { true, true, true };

const float DELTA_POS   = 0.05f;
const float DELTA_ESC   = 0.1f;
const float ESC_MINIMA  = 0.05f;
const float VEL_GIRO    = 1.5f;

// Recursos do HUD
GLuint hudShader = 0;
GLuint hudVAO = 0, hudVBO = 0, hudEBO = 0;
GLint  hudProj = -1, hudCor = -1;


// --------------------------------------------------------------------------
// MAIN
// --------------------------------------------------------------------------
int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* janela = glfwCreateWindow(LARGURA, ALTURA,
        "CG - Raphael Ferracioli", nullptr, nullptr);
    glfwMakeContextCurrent(janela);
    glfwSetKeyCallback(janela, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Falha ao carregar GLAD" << endl;
        return -1;
    }

    int fbL, fbA;
    glfwGetFramebufferSize(janela, &fbL, &fbA);
    glViewport(0, 0, fbL, fbA);

    GLuint prog = setupShader();
    glUseProgram(prog);

    // Locations dos uniforms do shader principal
    GLint modelLoc     = glGetUniformLocation(prog, "model");
    GLint mapaLoc      = glGetUniformLocation(prog, "mapa");
    GLint posCamLoc    = glGetUniformLocation(prog, "posCamera");
    GLint kaLoc        = glGetUniformLocation(prog, "ka");
    GLint kdLoc        = glGetUniformLocation(prog, "kd");
    GLint ksLoc        = glGetUniformLocation(prog, "ks");
    GLint qLoc         = glGetUniformLocation(prog, "q");
    GLint realceLoc    = glGetUniformLocation(prog, "realce");

    GLint luzPosLoc[3], luzIntLoc[3], luzOnLoc[3];
    for (int i = 0; i < 3; i++)
    {
        luzPosLoc[i] = glGetUniformLocation(prog, ("lightPos[" + to_string(i) + "]").c_str());
        luzIntLoc[i] = glGetUniformLocation(prog, ("lightInt[" + to_string(i) + "]").c_str());
        luzOnLoc[i]  = glGetUniformLocation(prog, ("lightOn["  + to_string(i) + "]").c_str());
    }

    glUniform1i(mapaLoc,   0);
    glUniform3f(posCamLoc, 0.0f, 0.0f, 2.0f);

    float intensidades[3] = { 1.2f, 0.5f, 0.7f };
    for (int i = 0; i < 3; i++)
        glUniform1f(luzIntLoc[i], intensidades[i]);

    // Inicializa sistema de HUD
    hudShader = setupTextShader();
    glGenVertexArrays(1, &hudVAO);
    glGenBuffers(1, &hudVBO);
    glGenBuffers(1, &hudEBO);
    hudProj = glGetUniformLocation(hudShader, "projecao");
    hudCor  = glGetUniformLocation(hudShader, "corTexto");

    glEnable(GL_DEPTH_TEST);

    // Carrega modelos da cena
    int    nv = 0;
    GLuint tx = 0;
    float  ka = 0.2f, kd = 0.7f, ks = 0.5f, brilho = 32.0f;

    GLuint vaoA = (GLuint)carregarOBJ("assets/Modelos3D/Suzanne.obj", nv, tx, ka, kd, ks, brilho);
    if (vaoA != (GLuint)-1)
        cena.push_back(Malha(vaoA, nv, tx, ka, kd, ks, brilho, glm::vec3(-0.4f, 0.0f, 0.0f), "Suzanne"));

    ka = 0.2f; kd = 0.7f; ks = 0.5f; brilho = 32.0f;
    GLuint vaoB = (GLuint)carregarOBJ("assets/Modelos3D/Cube.obj", nv, tx, ka, kd, ks, brilho);
    if (vaoB != (GLuint)-1)
        cena.push_back(Malha(vaoB, nv, tx, ka, kd, ks, brilho, glm::vec3(0.4f, 0.0f, 0.0f), "Cubo"));

    if (cena.empty())
    {
        cout << "Nenhum modelo carregado." << endl;
        glfwTerminate();
        return -1;
    }

    float tAnterior = (float)glfwGetTime();

    while (!glfwWindowShouldClose(janela))
    {
        float tAtual = (float)glfwGetTime();
        float dt     = tAtual - tAnterior;
        tAnterior    = tAtual;

        glfwPollEvents();

        glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Atualiza posicoes das luzes e estado on/off
        atualizarLuzes(prog, luzPosLoc, luzOnLoc);

        // Desenha cada malha da cena
        glUseProgram(prog);
        for (int i = 0; i < (int)cena.size(); i++)
        {
            if (cena[i].girando)
                cena[i].angulo += VEL_GIRO * dt;

            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cena[i].pos);
            model = glm::rotate(model,    cena[i].angulo, cena[i].eixo);
            model = glm::scale(model,     cena[i].esc);

            glUniformMatrix4fv(modelLoc,  1, GL_FALSE, glm::value_ptr(model));
            glUniform1f(kaLoc,    cena[i].ka);
            glUniform1f(kdLoc,    cena[i].kd);
            glUniform1f(ksLoc,    cena[i].ks);
            glUniform1f(qLoc,     cena[i].brilho);
            glUniform1f(realceLoc, (i == atual) ? 1.2f : 1.0f);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, cena[i].textura);
            glBindVertexArray(cena[i].id);
            glDrawArrays(GL_TRIANGLES, 0, cena[i].qtdVerts);
        }
        glBindVertexArray(0);

        // Sobreposicao do HUD
        glDisable(GL_DEPTH_TEST);
        desenharHUD();
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(janela);
    }

    glfwTerminate();
    return 0;
}


// --------------------------------------------------------------------------
// Callback de teclado
// --------------------------------------------------------------------------
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mod)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
        atual = (atual + 1) % (int)cena.size();

    if (key == GLFW_KEY_R && action == GLFW_PRESS) modo = Modo::SPIN;
    if (key == GLFW_KEY_T && action == GLFW_PRESS) modo = Modo::SLIDE;
    if (key == GLFW_KEY_S && action == GLFW_PRESS) modo = Modo::RESIZE;

    if (key == GLFW_KEY_1 && action == GLFW_PRESS) lucesAtivas[0] = !lucesAtivas[0];
    if (key == GLFW_KEY_2 && action == GLFW_PRESS) lucesAtivas[1] = !lucesAtivas[1];
    if (key == GLFW_KEY_3 && action == GLFW_PRESS) lucesAtivas[2] = !lucesAtivas[2];

    if (modo == Modo::SPIN && action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_X) { cena[atual].eixo = glm::vec3(1,0,0); cena[atual].girando = true; }
        if (key == GLFW_KEY_Y) { cena[atual].eixo = glm::vec3(0,1,0); cena[atual].girando = true; }
        if (key == GLFW_KEY_Z) { cena[atual].eixo = glm::vec3(0,0,1); cena[atual].girando = true; }
    }

    if (modo == Modo::SLIDE && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        glm::vec3& p = cena[atual].pos;
        if (key == GLFW_KEY_LEFT)  p.x -= DELTA_POS;
        if (key == GLFW_KEY_RIGHT) p.x += DELTA_POS;
        if (key == GLFW_KEY_UP)    p.y += DELTA_POS;
        if (key == GLFW_KEY_DOWN)  p.y -= DELTA_POS;
        if (key == GLFW_KEY_W) p.z = glm::max(p.z - DELTA_POS, -0.5f);
        if (key == GLFW_KEY_E) p.z = glm::min(p.z + DELTA_POS,  0.5f);
    }

    if (modo == Modo::RESIZE && (action == GLFW_PRESS || action == GLFW_REPEAT))
    {
        glm::vec3& e = cena[atual].esc;
        if (key == GLFW_KEY_EQUAL) e += glm::vec3(DELTA_ESC);
        if (key == GLFW_KEY_MINUS)
        {
            e -= glm::vec3(DELTA_ESC);
            if (e.x < ESC_MINIMA) e = glm::vec3(ESC_MINIMA);
        }
    }
}


// --------------------------------------------------------------------------
// Desenha o HUD na tela
// --------------------------------------------------------------------------
void desenharHUD()
{
    const float TAM    = 2.0f;
    const float LINHA  = 18.0f;

    // objeto selecionado (centralizado no topo) ──────────────────────────
    string titulo = "< " + cena[atual].rotulo + " >";
    float xCentro = (LARGURA - titulo.size() * 6.0f * TAM * 0.5f) * 0.5f;
    renderText(titulo, xCentro, 12, TAM, glm::vec3(1.0f, 1.0f, 0.8f));

    // modo ativo (lookup por indice, sem switch) ─────────────────────────
    const char* descModo[] = { "Inativo", "Girando (X/Y/Z)", "Movendo (setas | W/E)", "Escalando (= / -)" };
    glm::vec3   corModo[]  = {
        glm::vec3(0.5f,  0.5f,  0.5f),
        glm::vec3(1.0f,  0.75f, 0.1f),
        glm::vec3(0.2f,  1.0f,  0.45f),
        glm::vec3(0.35f, 0.6f,  1.0f)
    };
    int idx = (int)modo;
    renderText(string(">> ") + descModo[idx], 10, 40, TAM, corModo[idx]);

    // estado das luzes (loop) ────────────────────────────────────────────
    const char* nomeLuz[] = { "Principal", "Enchimento", "Contra-luz" };
    glm::vec3 corLigada  = glm::vec3(0.25f, 1.0f,  0.45f);
    glm::vec3 corApagada = glm::vec3(0.38f, 0.38f, 0.38f);
    for (int i = 0; i < 3; i++)
    {
        bool ativa = lucesAtivas[i];
        string luzStr = "[" + to_string(i + 1) + "] " + nomeLuz[i] + ": " + (ativa ? "ON" : "OFF");
        renderText(luzStr, 10, 68 + i * LINHA, TAM, ativa ? corLigada : corApagada);
    }

    // ajuda no rodape (loop) ─────────────────────────────────────────────
    const char* ajuda[] = {
        "TAB    trocar objeto",
        "R / T / S    girar / mover / escalar",
        "1 / 2 / 3    alternar luzes",
        "ESC    encerrar"
    };
    int nLinhas   = 4;
    float yInicio = ALTURA - nLinhas * LINHA - 14.0f;
    glm::vec3 corAjuda = glm::vec3(0.58f, 0.58f, 0.58f);
    for (int i = 0; i < nLinhas; i++)
        renderText(ajuda[i], 10, yInicio + i * LINHA, TAM, corAjuda);
}


// --------------------------------------------------------------------------
// Atualiza posicao e estado das tres luzes no shader
// --------------------------------------------------------------------------
void atualizarLuzes(GLuint prog, GLint posLoc[], GLint onLoc[])
{
    glm::vec3 ref = cena[0].pos;
    float     mag = glm::length(cena[0].esc);

    glm::vec3 posicoes[3] = {
        ref + glm::vec3(-mag * 1.5f,  mag * 1.5f,  mag * 2.5f),
        ref + glm::vec3( mag * 1.5f,  mag * 0.5f,  mag * 2.5f),
        ref + glm::vec3( 0.0f,         mag * 1.0f, -mag * 2.5f)
    };

    glUseProgram(prog);
    for (int i = 0; i < 3; i++)
    {
        glUniform3fv(posLoc[i], 1, glm::value_ptr(posicoes[i]));
        glUniform1i(onLoc[i], lucesAtivas[i] ? 1 : 0);
    }
}


// --------------------------------------------------------------------------
// Renderiza texto 2D usando stb_easy_font
// --------------------------------------------------------------------------
void renderText(const string& str, float x, float y, float tam, glm::vec3 cor)
{
    static char buf[99999];
    int nQuads = stb_easy_font_print(0, 0, (char*)str.c_str(), nullptr, buf, sizeof(buf));

    int nVerts = nQuads * 4;
    vector<float> verts(nVerts * 2);
    for (int i = 0; i < nVerts; i++)
    {
        float* src = (float*)(buf + i * 16);
        verts[i * 2 + 0] = src[0] * tam + x;
        verts[i * 2 + 1] = src[1] * tam + y;
    }

    vector<unsigned int> idx;
    idx.reserve(nQuads * 6);
    for (int q = 0; q < nQuads; q++)
    {
        int b = q * 4;
        idx.push_back(b+0); idx.push_back(b+1); idx.push_back(b+2);
        idx.push_back(b+0); idx.push_back(b+2); idx.push_back(b+3);
    }

    glm::mat4 proj = glm::ortho(0.0f, (float)LARGURA, (float)ALTURA, 0.0f, -1.0f, 1.0f);

    glUseProgram(hudShader);
    glUniformMatrix4fv(hudProj, 1, GL_FALSE, glm::value_ptr(proj));
    glUniform3fv(hudCor,  1, glm::value_ptr(cor));

    glBindVertexArray(hudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, hudVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, hudEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, idx.size() * sizeof(unsigned int), idx.data(), GL_DYNAMIC_DRAW);
    glDrawElements(GL_TRIANGLES, (GLsizei)idx.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}


// --------------------------------------------------------------------------
// Compilacao de shaders
// --------------------------------------------------------------------------
static GLuint compilarShader(GLenum tipo, const GLchar* src)
{
    GLuint s = glCreateShader(tipo);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok; GLchar log[512];
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(s, 512, NULL, log); cout << log << endl; }
    return s;
}

int setupShader()
{
    GLuint vs   = compilarShader(GL_VERTEX_SHADER,   srcVertPrincipal);
    GLuint fs   = compilarShader(GL_FRAGMENT_SHADER, srcFragPrincipal);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}

int setupTextShader()
{
    GLuint vs   = compilarShader(GL_VERTEX_SHADER,   srcVertTexto);
    GLuint fs   = compilarShader(GL_FRAGMENT_SHADER, srcFragTexto);
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs); glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs); glDeleteShader(fs);
    return prog;
}


// --------------------------------------------------------------------------
// Carregamento de textura
// --------------------------------------------------------------------------
GLuint carregarTextura(const string& caminho)
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_set_flip_vertically_on_load(true);
    int w, h, canais;
    unsigned char* pixels = stbi_load(caminho.c_str(), &w, &h, &canais, 0);
    if (pixels)
    {
        GLenum fmt = (canais == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, w, h, 0, fmt, GL_UNSIGNED_BYTE, pixels);
        glGenerateMipmap(GL_TEXTURE_2D);
        cout << "Textura: " << caminho << " (" << w << "x" << h << ")" << endl;
    }
    else
    {
        unsigned char branco[] = { 255, 255, 255, 255 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, branco);
        cout << "Textura nao encontrada: " << caminho << endl;
    }
    stbi_image_free(pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}


// --------------------------------------------------------------------------
// Leitura do arquivo .mtl
// --------------------------------------------------------------------------
static string lerMTL(const string& caminhoMtl, const string& dirBase,
                     float& ka, float& kd, float& ks, float& brilho)
{
    ifstream fileMtl(caminhoMtl.c_str());
    if (!fileMtl.is_open()) return "";

    string linha, nomeTextura;
    while (getline(fileMtl, linha))
    {
        istringstream ss(linha);
        string chave;
        ss >> chave;

        if      (chave == "Ka") { float r,g,b; ss>>r>>g>>b; ka     = (r+g+b)/3.0f; }
        else if (chave == "Kd") { float r,g,b; ss>>r>>g>>b; kd     = (r+g+b)/3.0f; }
        else if (chave == "Ks") { float r,g,b; ss>>r>>g>>b; ks     = (r+g+b)/3.0f; }
        else if (chave == "Ns") { ss >> brilho; }
        else if (chave == "map_Kd") { ss >> nomeTextura; }
    }
    return nomeTextura.empty() ? "" : dirBase + nomeTextura;
}


// --------------------------------------------------------------------------
// Carregamento de arquivo .obj
// --------------------------------------------------------------------------
int carregarOBJ(const string& caminho, int& qtdVerts, GLuint& texID,
                float& ka, float& kd, float& ks, float& brilho)
{
    string dirBase;
    size_t sep = caminho.find_last_of("/\\");
    if (sep != string::npos) dirBase = caminho.substr(0, sep + 1);

    vector<glm::vec3> posVertices;
    vector<glm::vec2> coordsUV;
    vector<glm::vec3> normaisVert;
    vector<GLfloat>   dadosVertices;
    string            arquivoMtl;

    ifstream arquivo(caminho.c_str());
    if (!arquivo.is_open())
    {
        cerr << "Erro ao abrir: " << caminho << endl;
        return -1;
    }

    string linha;
    while (getline(arquivo, linha))
    {
        istringstream linhaSS(linha);
        string token;
        linhaSS >> token;

        if (token == "mtllib")
        {
            linhaSS >> arquivoMtl;
        }
        else if (token == "v")
        {
            glm::vec3 v;
            linhaSS >> v.x >> v.y >> v.z;
            posVertices.push_back(v);
        }
        else if (token == "vt")
        {
            glm::vec2 vt;
            linhaSS >> vt.s >> vt.t;
            coordsUV.push_back(vt);
        }
        else if (token == "vn")
        {
            glm::vec3 vn;
            linhaSS >> vn.x >> vn.y >> vn.z;
            normaisVert.push_back(vn);
        }
        else if (token == "f")
        {
            struct IndicesFace { int vi, ti, ni; };
            vector<IndicesFace> face;
            string parte;
            while (linhaSS >> parte)
            {
                istringstream ss(parte);
                string vs, ts, ns;
                getline(ss, vs, '/');
                getline(ss, ts, '/');
                getline(ss, ns, '/');
                IndicesFace fv;
                fv.vi = vs.empty() ? 0 : stoi(vs) - 1;
                fv.ti = ts.empty() ? 0 : stoi(ts) - 1;
                fv.ni = ns.empty() ? 0 : stoi(ns) - 1;
                face.push_back(fv);
            }
            // Triangulacao em leque
            for (int j = 1; j + 1 < (int)face.size(); j++)
            {
                int tri[3] = { 0, j, j + 1 };
                for (int k = 0; k < 3; k++)
                {
                    glm::vec3 p = posVertices[face[tri[k]].vi];
                    glm::vec2 t = coordsUV.empty()    ? glm::vec2(0.0f)           : coordsUV[face[tri[k]].ti];
                    glm::vec3 n = normaisVert.empty()  ? glm::vec3(0.0f, 0.0f, 1.0f) : normaisVert[face[tri[k]].ni];
                    dadosVertices.push_back(p.x); dadosVertices.push_back(p.y); dadosVertices.push_back(p.z);
                    dadosVertices.push_back(t.s); dadosVertices.push_back(t.t);
                    dadosVertices.push_back(n.x); dadosVertices.push_back(n.y); dadosVertices.push_back(n.z);
                }
            }
        }
    }
    arquivo.close();

    string caminhoTex;
    if (!arquivoMtl.empty())
        caminhoTex = lerMTL(dirBase + arquivoMtl, dirBase, ka, kd, ks, brilho);
    texID = carregarTextura(caminhoTex.empty() ? "" : caminhoTex);

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, dadosVertices.size() * sizeof(GLfloat), dadosVertices.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // posicao (x, y, z)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    // coordenada UV (s, t)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    // normal (nx, ny, nz)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    qtdVerts = (int)(dadosVertices.size() / 8);
    return VAO;
}
