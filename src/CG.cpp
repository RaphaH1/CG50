/* Computacao Grafica - Unisinos
 * Visualizador de cenas 3D
 *
 * Camera (primeira pessoa):
 *   W A S D       - movimenta a camera
 *   SPACE / SHIFT - sobe / desce a camera
 *   mouse         - orienta a camera
 *
 * Objeto selecionado:
 *   TAB           - seleciona o proximo objeto da cena
 *   setas         - translada em X (esq/dir) e em Z (cima/baixo)
 *   PAGE UP/DOWN  - translada em Y
 *   X, Y, Z       - liga/desliga a rotacao no respectivo eixo
 *   [ / ]         - diminui / aumenta a escala (uniforme)
 *
 * Iluminacao:
 *   1, 2, 3       - liga/desliga cada uma das tres luzes
 *
 * Trajetoria (curva de Bezier):
 *   B             - adiciona um ponto de controle na posicao do objeto
 *   N             - inicia / pausa a animacao
 *   C             - limpa os pontos de controle
 *   G             - grava a cena no arquivo de configuracao
 *
 *   ESC           - encerra
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>

using namespace std;

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// --------------------------------------------------------------------------
// Prototipos
// --------------------------------------------------------------------------
void   key_callback(GLFWwindow* window, int key, int scancode, int action, int mod);
void   mouse_callback(GLFWwindow* window, double xpos, double ypos);
void   processarCamera(GLFWwindow* window, float dt);
int    setupShader();
void   atualizarLuzes(GLuint prog, GLint posLoc[], GLint onLoc[]);
bool   carregarCena(const string& caminho);
void   salvarCena(const string& caminho);
int    carregarOBJ(const string& caminho, int& qtdVerts, GLuint& texID,
                   float& ka, float& kd, float& ks, float& brilho);
GLuint carregarTextura(const string& caminho);
GLuint criarTexturaBranca();
GLuint criarCubo(glm::vec3 corTopo, glm::vec3 corLado, glm::vec3 corBaixo, int& qtdVerts);
void   gerarChao(int n, float tam);

// --------------------------------------------------------------------------
// Dimensoes da janela
// --------------------------------------------------------------------------
const GLuint LARGURA = 1000, ALTURA = 1000;

// --------------------------------------------------------------------------
// Vertex Shader — aplica model/view/projection e passa atributos ao fragment
// --------------------------------------------------------------------------
const GLchar* srcVertPrincipal = R"(
#version 330
layout (location = 0) in vec3 posicao;
layout (location = 1) in vec2 uvCoord;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec3 corVertice;
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
out vec2  coordUV;
out vec3  normalFrag;
out vec3  posicaoFrag;
out vec3  corVert;
void main()
{
    vec4 posM = model * vec4(posicao, 1.0);
    gl_Position  = projection * view * posM;
    posicaoFrag  = vec3(posM);
    coordUV      = uvCoord;
    corVert      = corVertice;
    normalFrag   = normalize(mat3(transpose(inverse(model))) * normal);
}
)";

// Fragment Shader — calcula iluminacao de Phong para ate 3 luzes
const GLchar* srcFragPrincipal = R"(
#version 330
in vec2  coordUV;
in vec3  normalFrag;
in vec3  posicaoFrag;
in vec3  corVert;
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
    vec3 corBase   = vec3(texture(mapa, coordUV)) * corVert;
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
        float atenuacao   = 1.0 / (1.0 + 0.14 * distancia + 0.07 * distancia * distancia);

        float fatorDifuso = max(dot(norm, direcaoLuz), 0.0);
        difusa   += kd * fatorDifuso * atenuacao * lightInt[i] * corBase;

        vec3  reflexo      = reflect(-direcaoLuz, norm);
        float fatorEspec   = pow(max(dot(reflexo, visao), 0.0), q);
        especular         += ks * fatorEspec * lightInt[i];
    }

    fragColor = vec4((ambiental + difusa + especular) * realce, 1.0);
}
)";

// --------------------------------------------------------------------------
// Camera em primeira pessoa
// --------------------------------------------------------------------------
enum Movimento { FRENTE, TRAS, ESQUERDA, DIREITA, CIMA, BAIXO };

class Camera
{
public:
    glm::vec3 posicao;
    glm::vec3 frente;
    glm::vec3 cima;
    glm::vec3 direita;
    glm::vec3 cimaMundo;
    float yaw;
    float pitch;
    float velocidade;
    float sensibilidade;

    Camera(glm::vec3 pos = glm::vec3(0.0f, 0.0f, 3.0f),
           float yawInicial = -90.0f, float pitchInicial = 0.0f)
        : frente(glm::vec3(0.0f, 0.0f, -1.0f)), velocidade(2.5f), sensibilidade(0.1f)
    {
        posicao   = pos;
        cimaMundo = glm::vec3(0.0f, 1.0f, 0.0f);
        yaw       = yawInicial;
        pitch     = pitchInicial;
        atualizarVetores();
    }

    glm::mat4 matrizVisao() const
    {
        return glm::lookAt(posicao, posicao + frente, cima);
    }

    void mover(Movimento dir, float dt)
    {
        float v = velocidade * dt;
        if (dir == FRENTE)   posicao += frente    * v;
        if (dir == TRAS)     posicao -= frente    * v;
        if (dir == ESQUERDA) posicao -= direita   * v;
        if (dir == DIREITA)  posicao += direita   * v;
        if (dir == CIMA)     posicao += cimaMundo * v;
        if (dir == BAIXO)    posicao -= cimaMundo * v;
    }

    void girar(float dx, float dy)
    {
        yaw   += dx * sensibilidade;
        pitch += dy * sensibilidade;
        if (pitch >  89.0f) pitch =  89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
        atualizarVetores();
    }

private:
    void atualizarVetores()
    {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        frente  = glm::normalize(f);
        direita = glm::normalize(glm::cross(frente, cimaMundo));
        cima    = glm::normalize(glm::cross(direita, frente));
    }
};

// --------------------------------------------------------------------------
// Estrutura de Trajetoria — curva de Bezier (de Casteljau)
// --------------------------------------------------------------------------
struct Trajetoria
{
    vector<glm::vec3> controle;     // pontos de controle informados pelo usuario
    vector<glm::vec3> curva;        // pontos amostrados ao longo da curva de Bezier
    float parametro = 0.0f;         // parametro de avanco (0.0 a 1.0)
    float velocidade = 0.2f;        // fracao da curva percorrida por segundo
    bool ativa = false;             // se a animacao esta em movimento

    void adicionarPonto(glm::vec3 p)
    {
        controle.push_back(p);
        gerarCurva();
    }

    void limpar()
    {
        controle.clear();
        curva.clear();
        parametro = 0.0f;
        ativa = false;
    }

    void iniciar()
    {
        if (curva.size() > 1)
        {
            ativa = true;
            parametro = 0.0f;
        }
    }

    void parar()
    {
        ativa = false;
    }

    bool estaVazia() const
    {
        return controle.empty();
    }

    int qtdPontos() const
    {
        return (int)controle.size();
    }

    // Algoritmo de de Casteljau: avalia a curva de Bezier no parametro t,
    // interpolando recursivamente os pontos de controle
    glm::vec3 deCasteljau(float t) const
    {
        vector<glm::vec3> tmp = controle;
        int n = (int)tmp.size();
        for (int k = 1; k < n; k++)
            for (int i = 0; i < n - k; i++)
                tmp[i] = (1.0f - t) * tmp[i] + t * tmp[i + 1];
        return tmp[0];
    }

    // Amostra a curva de Bezier numa poligonal densa para a animacao
    void gerarCurva()
    {
        curva.clear();
        if (controle.size() < 2) return;
        const int amostras = 200;
        for (int i = 0; i <= amostras; i++)
        {
            float t = (float)i / (float)amostras;
            curva.push_back(deCasteljau(t));
        }
    }

    // Avanca o objeto pela curva de forma ciclica (ao terminar, volta ao inicio)
    glm::vec3 atualizarPosicao(float dt)
    {
        if (!ativa || curva.size() < 2)
            return curva.empty() ? glm::vec3(0.0f) : curva.front();

        parametro += velocidade * dt;
        while (parametro >= 1.0f) parametro -= 1.0f;

        float f = parametro * (float)(curva.size() - 1);
        int i = (int)f;
        if (i >= (int)curva.size() - 1) i = (int)curva.size() - 2;
        float frac = f - (float)i;
        return curva[i] * (1.0f - frac) + curva[i + 1] * frac;
    }
};

// --------------------------------------------------------------------------
// Estado da aplicacao
// --------------------------------------------------------------------------
struct Malha
{
    GLuint      id;
    int         qtdVerts;
    GLuint      textura;
    float       ka, kd, ks, brilho;
    glm::vec3   pos;
    glm::vec3   esc;
    glm::vec3   eixo;
    float       angulo;
    bool        girando;
    string      rotulo;
    string      caminho;           // caminho do .obj (usado ao gravar a cena)
    Trajetoria  trajetoria;        // trajetoria do objeto

    Malha(GLuint id, int qtdVerts, GLuint tex,
          float ka, float kd, float ks, float brilho,
          glm::vec3 pos, string rotulo)
        : id(id), qtdVerts(qtdVerts), textura(tex),
          ka(ka), kd(kd), ks(ks), brilho(brilho),
          pos(pos), esc(glm::vec3(0.3f)),
          eixo(glm::vec3(0.0f, 1.0f, 0.0f)),
          angulo(0.0f), girando(false), rotulo(rotulo) {}
};

// Bloco do chao do diorama (geometria colorida, sem selecao nem trajetoria)
struct Bloco
{
    GLuint    vao;
    int       qtdVerts;
    glm::vec3 pos;
    float     tam;
};

vector<Malha> cena;
vector<Bloco> chao;
GLuint        texBranca = 0;
int           atual          = 0;
bool          lucesAtivas[3] = { true, true, true };
float         intensidadeLuz[3] = { 1.2f, 0.5f, 0.7f };

// Material do chao (coeficientes de Phong dos blocos)
const float CHAO_KA = 0.45f, CHAO_KD = 0.85f, CHAO_KS = 0.08f, CHAO_Q = 8.0f;

// Camera e frustrum
Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float  fovCamera = 45.0f;
float  znear     = 0.1f;
float  zfar      = 100.0f;

// Controle do mouse
float ultimoX     = LARGURA / 2.0f;
float ultimoY     = ALTURA  / 2.0f;
bool  primeiroMouse = true;

// Arquivo de configuracao de cena
const string ARQUIVO_CENA = "cena.txt";

const float DELTA_POS   = 0.05f;
const float DELTA_ESC   = 0.1f;
const float ESC_MINIMA  = 0.05f;
const float VEL_GIRO    = 1.5f;


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
    glfwSetCursorPosCallback(janela, mouse_callback);
    glfwSetInputMode(janela, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
    GLint viewLoc      = glGetUniformLocation(prog, "view");
    GLint projLoc      = glGetUniformLocation(prog, "projection");
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

    glUniform1i(mapaLoc, 0);

    glEnable(GL_DEPTH_TEST);

    // Carrega a cena a partir do arquivo de configuracao
    if (!carregarCena(ARQUIVO_CENA))
    {
        cout << "Nenhum modelo carregado. Verifique o arquivo " << ARQUIVO_CENA << endl;
        glfwTerminate();
        return -1;
    }

    // Envia as intensidades das luzes (definidas no arquivo de configuracao)
    for (int i = 0; i < 3; i++)
        glUniform1f(luzIntLoc[i], intensidadeLuz[i]);

    cout << "Cena carregada com " << cena.size() << " objeto(s)." << endl;
    cout << "Use WASD + mouse para a camera, TAB para selecionar e ESC para sair." << endl;

    float aspecto = (float)fbL / (float)fbA;
    float tAnterior = (float)glfwGetTime();

    while (!glfwWindowShouldClose(janela))
    {
        float tAtual = (float)glfwGetTime();
        float dt     = tAtual - tAnterior;
        tAnterior    = tAtual;

        glfwPollEvents();
        processarCamera(janela, dt);

        glClearColor(0.08f, 0.08f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Atualiza posicoes das luzes e estado on/off
        atualizarLuzes(prog, luzPosLoc, luzOnLoc);

        glUseProgram(prog);

        // Matrizes de camera (view) e projecao (frustrum)
        glm::mat4 view = camera.matrizVisao();
        glm::mat4 projecao = glm::perspective(glm::radians(fovCamera), aspecto, znear, zfar);
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projecao));
        glUniform3fv(posCamLoc, 1, glm::value_ptr(camera.posicao));

        // Desenha os blocos do chao (diorama)
        glUniform1f(kaLoc, CHAO_KA);
        glUniform1f(kdLoc, CHAO_KD);
        glUniform1f(ksLoc, CHAO_KS);
        glUniform1f(qLoc,  CHAO_Q);
        glUniform1f(realceLoc, 1.0f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texBranca);
        for (size_t i = 0; i < chao.size(); i++)
        {
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, chao[i].pos);
            model = glm::scale(model, glm::vec3(chao[i].tam));
            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glBindVertexArray(chao[i].vao);
            glDrawArrays(GL_TRIANGLES, 0, chao[i].qtdVerts);
        }

        // Desenha cada malha da cena
        for (int i = 0; i < (int)cena.size(); i++)
        {
            if (cena[i].girando)
                cena[i].angulo += VEL_GIRO * dt;

            // Atualiza posicao se a trajetoria esta ativa
            if (cena[i].trajetoria.ativa && !cena[i].trajetoria.estaVazia())
            {
                cena[i].pos = cena[i].trajetoria.atualizarPosicao(dt);
            }

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

    // Seleciona o proximo objeto da cena
    if (key == GLFW_KEY_TAB && action == GLFW_PRESS)
    {
        atual = (atual + 1) % (int)cena.size();
        cout << "Selecionado: " << cena[atual].rotulo << endl;
    }

    // Liga/desliga cada uma das tres luzes
    if (key == GLFW_KEY_1 && action == GLFW_PRESS) lucesAtivas[0] = !lucesAtivas[0];
    if (key == GLFW_KEY_2 && action == GLFW_PRESS) lucesAtivas[1] = !lucesAtivas[1];
    if (key == GLFW_KEY_3 && action == GLFW_PRESS) lucesAtivas[2] = !lucesAtivas[2];

    // Rotacao do objeto selecionado (liga/desliga em cada eixo)
    if (action == GLFW_PRESS && (key == GLFW_KEY_X || key == GLFW_KEY_Y || key == GLFW_KEY_Z))
    {
        glm::vec3 e = (key == GLFW_KEY_X) ? glm::vec3(1, 0, 0)
                    : (key == GLFW_KEY_Y) ? glm::vec3(0, 1, 0)
                                          : glm::vec3(0, 0, 1);
        Malha& m = cena[atual];
        if (m.girando && m.eixo == e) m.girando = false;
        else { m.eixo = e; m.girando = true; }
    }

    // Trajetoria por curva de Bezier
    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_B)   // adiciona ponto de controle na posicao do objeto
        {
            cena[atual].trajetoria.adicionarPonto(cena[atual].pos);
            cout << "Ponto de controle adicionado ("
                 << cena[atual].trajetoria.qtdPontos() << ")" << endl;
        }
        if (key == GLFW_KEY_N)   // inicia/pausa a animacao
        {
            if (cena[atual].trajetoria.ativa) cena[atual].trajetoria.parar();
            else                              cena[atual].trajetoria.iniciar();
        }
        if (key == GLFW_KEY_C)   // limpa os pontos de controle
        {
            cena[atual].trajetoria.limpar();
            cout << "Trajetoria limpa" << endl;
        }
        if (key == GLFW_KEY_G)   // grava a cena no arquivo de configuracao
        {
            salvarCena(ARQUIVO_CENA);
        }
    }

    // Translacao do objeto selecionado
    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        glm::vec3& p = cena[atual].pos;
        if (key == GLFW_KEY_LEFT)      p.x -= DELTA_POS;
        if (key == GLFW_KEY_RIGHT)     p.x += DELTA_POS;
        if (key == GLFW_KEY_UP)        p.z -= DELTA_POS;
        if (key == GLFW_KEY_DOWN)      p.z += DELTA_POS;
        if (key == GLFW_KEY_PAGE_UP)   p.y += DELTA_POS;
        if (key == GLFW_KEY_PAGE_DOWN) p.y -= DELTA_POS;

        // Escala uniforme do objeto selecionado
        glm::vec3& e = cena[atual].esc;
        if (key == GLFW_KEY_RIGHT_BRACKET) e += glm::vec3(DELTA_ESC);
        if (key == GLFW_KEY_LEFT_BRACKET)
        {
            e -= glm::vec3(DELTA_ESC);
            if (e.x < ESC_MINIMA) e = glm::vec3(ESC_MINIMA);
        }
    }
}


// --------------------------------------------------------------------------
// Callback do mouse — orienta a camera
// --------------------------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
    if (primeiroMouse)
    {
        ultimoX = (float)xpos;
        ultimoY = (float)ypos;
        primeiroMouse = false;
    }

    float dx = (float)xpos - ultimoX;
    float dy = ultimoY - (float)ypos;   // invertido: y cresce para baixo na tela
    ultimoX = (float)xpos;
    ultimoY = (float)ypos;

    camera.girar(dx, dy);
}


// --------------------------------------------------------------------------
// Movimentacao da camera (lida a cada quadro para um movimento suave)
// --------------------------------------------------------------------------
void processarCamera(GLFWwindow* window, float dt)
{
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.mover(FRENTE,  dt);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.mover(TRAS,    dt);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camera.mover(ESQUERDA, dt);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camera.mover(DIREITA, dt);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)      camera.mover(CIMA, dt);
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camera.mover(BAIXO, dt);
}


// --------------------------------------------------------------------------
// Atualiza posicao e estado das tres luzes no shader.
// As luzes sao posicionadas automaticamente a partir do objeto principal
// (cena[0]) seguindo a tecnica de iluminacao de tres pontos.
// --------------------------------------------------------------------------
void atualizarLuzes(GLuint prog, GLint posLoc[], GLint onLoc[])
{
    // Posiciona as tres luzes a partir do objeto principal (cena[0]).
    // As distancias sao calibradas para iluminar o objeto e a area do mapa
    // ao seu redor, seguindo a tecnica de iluminacao de tres pontos.
    glm::vec3 ref = cena[0].pos;

    glm::vec3 posicoes[3] = {
        ref + glm::vec3(-1.5f, 2.0f,  1.5f),  // principal (key)
        ref + glm::vec3( 1.5f, 1.2f,  1.2f),  // preenchimento (fill)
        ref + glm::vec3( 0.0f, 2.0f, -2.0f)   // contra-luz (back)
    };

    glUseProgram(prog);
    for (int i = 0; i < 3; i++)
    {
        glUniform3fv(posLoc[i], 1, glm::value_ptr(posicoes[i]));
        glUniform1i(onLoc[i], lucesAtivas[i] ? 1 : 0);
    }
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
// Cria uma textura 1x1 branca, usada nos objetos coloridos sem mapa de textura
// (a cor final vem do atributo de cor do vertice)
// --------------------------------------------------------------------------
GLuint criarTexturaBranca()
{
    GLuint id;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    unsigned char branco[] = { 255, 255, 255, 255 };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, branco);
    glBindTexture(GL_TEXTURE_2D, 0);
    return id;
}


// --------------------------------------------------------------------------
// Cria um cubo (lado 1, centrado na origem) com cor por face:
// topo, laterais e base podem receber cores diferentes.
// Layout do vertice: posicao(3) + uv(2) + normal(3) + cor(3) = 11 floats
// --------------------------------------------------------------------------
GLuint criarCubo(glm::vec3 corTopo, glm::vec3 corLado, glm::vec3 corBaixo, int& qtdVerts)
{
    const float h = 0.5f;

    struct Face { glm::vec3 n; glm::vec3 c[4]; glm::vec3 cor; };
    glm::vec3 cT = corTopo, cL = corLado, cB = corBaixo;

    Face faces[6] = {
        // frente (+z)
        { { 0, 0, 1}, { {-h,-h, h}, { h,-h, h}, { h, h, h}, {-h, h, h} }, cL },
        // tras (-z)
        { { 0, 0,-1}, { { h,-h,-h}, {-h,-h,-h}, {-h, h,-h}, { h, h,-h} }, cL },
        // esquerda (-x)
        { {-1, 0, 0}, { {-h,-h,-h}, {-h,-h, h}, {-h, h, h}, {-h, h,-h} }, cL },
        // direita (+x)
        { { 1, 0, 0}, { { h,-h, h}, { h,-h,-h}, { h, h,-h}, { h, h, h} }, cL },
        // topo (+y)
        { { 0, 1, 0}, { {-h, h, h}, { h, h, h}, { h, h,-h}, {-h, h,-h} }, cT },
        // base (-y)
        { { 0,-1, 0}, { {-h,-h,-h}, { h,-h,-h}, { h,-h, h}, {-h,-h, h} }, cB }
    };

    glm::vec2 uvs[4] = { {0,0}, {1,0}, {1,1}, {0,1} };
    int tri[6] = { 0, 1, 2, 0, 2, 3 };

    vector<GLfloat> dados;
    for (int f = 0; f < 6; f++)
    {
        for (int k = 0; k < 6; k++)
        {
            int v = tri[k];
            glm::vec3 p = faces[f].c[v];
            glm::vec2 t = uvs[v];
            glm::vec3 n = faces[f].n;
            glm::vec3 c = faces[f].cor;
            dados.push_back(p.x); dados.push_back(p.y); dados.push_back(p.z);
            dados.push_back(t.x); dados.push_back(t.y);
            dados.push_back(n.x); dados.push_back(n.y); dados.push_back(n.z);
            dados.push_back(c.r); dados.push_back(c.g); dados.push_back(c.b);
        }
    }

    GLuint VBO, VAO;
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, dados.size() * sizeof(GLfloat), dados.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(8 * sizeof(GLfloat)));
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    qtdVerts = 36;
    return VAO;
}


// --------------------------------------------------------------------------
// Gera o chao do diorama: um tabuleiro n x n de blocos de grama (topo verde,
// laterais de terra) centrado na origem, com o topo no plano y = 0, cercado
// por uma borda de blocos de pedra um nivel acima.
// --------------------------------------------------------------------------
void gerarChao(int n, float tam)
{
    if (texBranca == 0) texBranca = criarTexturaBranca();

    int nv = 0;
    // Tres tons de verde para dar variacao natural a grama
    GLuint grama[3];
    grama[0] = criarCubo(glm::vec3(0.28f, 0.62f, 0.22f), glm::vec3(0.45f, 0.32f, 0.18f), glm::vec3(0.28f, 0.20f, 0.10f), nv);
    grama[1] = criarCubo(glm::vec3(0.34f, 0.68f, 0.26f), glm::vec3(0.47f, 0.34f, 0.20f), glm::vec3(0.28f, 0.20f, 0.10f), nv);
    grama[2] = criarCubo(glm::vec3(0.24f, 0.56f, 0.18f), glm::vec3(0.43f, 0.30f, 0.16f), glm::vec3(0.28f, 0.20f, 0.10f), nv);

    int nvP = 0;
    GLuint pedra = criarCubo(glm::vec3(0.58f, 0.58f, 0.60f), glm::vec3(0.46f, 0.46f, 0.50f), glm::vec3(0.34f, 0.34f, 0.38f), nvP);

    float meio = (n - 1) / 2.0f;

    // Campo de grama (topo em y = 0)
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
        {
            glm::vec3 p((i - meio) * tam, -tam * 0.5f, (j - meio) * tam);
            int t = (i * 7 + j * 3) % 3;
            chao.push_back({ grama[t], nv, p, tam });
        }

    // Borda de pedra um nivel acima, ao redor do campo
    for (int i = -1; i <= n; i++)
        for (int j = -1; j <= n; j++)
        {
            bool borda = (i == -1 || i == n || j == -1 || j == n);
            if (!borda) continue;
            glm::vec3 p((i - meio) * tam, tam * 0.5f, (j - meio) * tam);
            chao.push_back({ pedra, nvP, p, tam });
        }
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
                    // cor branca: a cor final vem da textura (corBase = textura * corVertice)
                    dadosVertices.push_back(1.0f); dadosVertices.push_back(1.0f); dadosVertices.push_back(1.0f);
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
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);
    // coordenada UV (s, t)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    // normal (nx, ny, nz)
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(5 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);
    // cor (r, g, b)
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid*)(8 * sizeof(GLfloat)));
    glEnableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    qtdVerts = (int)(dadosVertices.size() / 11);
    return VAO;
}


// --------------------------------------------------------------------------
// Extrai o nome do arquivo, sem diretorio e sem extensao
// --------------------------------------------------------------------------
static string nomeBase(const string& caminho)
{
    size_t s = caminho.find_last_of("/\\");
    string nome = (s == string::npos) ? caminho : caminho.substr(s + 1);
    size_t d = nome.find_last_of('.');
    if (d != string::npos) nome = nome.substr(0, d);
    return nome;
}


// --------------------------------------------------------------------------
// Leitura do arquivo de configuracao de cena
// Formato (uma diretiva por linha, # inicia comentario):
//   camera  px py pz  yaw pitch  fov near far
//   luz     indice  intensidade  ligada(0/1)
//   objeto  caminho.obj  px py pz  ex ey ez anguloGraus  escala
//   traj    px py pz                 (ponto de controle do ultimo objeto)
//   veltraj velocidade               (velocidade da trajetoria do ultimo objeto)
// --------------------------------------------------------------------------
bool carregarCena(const string& caminho)
{
    ifstream arq(caminho.c_str());
    if (!arq.is_open())
    {
        cerr << "Nao foi possivel abrir o arquivo de cena: " << caminho << endl;
        return false;
    }

    string linha;
    int ultimoObj = -1;

    while (getline(arq, linha))
    {
        size_t c = linha.find('#');
        if (c != string::npos) linha = linha.substr(0, c);

        istringstream ss(linha);
        string tipo;
        if (!(ss >> tipo)) continue;

        if (tipo == "camera")
        {
            glm::vec3 p;
            float yaw, pitch;
            ss >> p.x >> p.y >> p.z >> yaw >> pitch >> fovCamera >> znear >> zfar;
            camera = Camera(p, yaw, pitch);
        }
        else if (tipo == "luz")
        {
            int idx, on;
            float inten;
            ss >> idx >> inten >> on;
            if (idx >= 0 && idx < 3)
            {
                intensidadeLuz[idx] = inten;
                lucesAtivas[idx] = (on != 0);
            }
        }
        else if (tipo == "objeto")
        {
            string arquivo;
            glm::vec3 p, eixo;
            float ang, escala;
            ss >> arquivo >> p.x >> p.y >> p.z
               >> eixo.x >> eixo.y >> eixo.z >> ang >> escala;

            int nv = 0;
            GLuint tx = 0;
            float ka = 0.2f, kd = 0.7f, ks = 0.5f, brilho = 32.0f;
            GLuint vao = (GLuint)carregarOBJ(arquivo, nv, tx, ka, kd, ks, brilho);
            if (vao != (GLuint)-1)
            {
                Malha m(vao, nv, tx, ka, kd, ks, brilho, p, nomeBase(arquivo));
                m.esc     = glm::vec3(escala);
                m.angulo  = glm::radians(ang);
                m.caminho = arquivo;
                if (glm::length(eixo) > 0.0f) m.eixo = glm::normalize(eixo);
                cena.push_back(m);
                ultimoObj = (int)cena.size() - 1;
            }
        }
        else if (tipo == "traj")
        {
            glm::vec3 p;
            ss >> p.x >> p.y >> p.z;
            if (ultimoObj >= 0) cena[ultimoObj].trajetoria.adicionarPonto(p);
        }
        else if (tipo == "veltraj")
        {
            float v;
            ss >> v;
            if (ultimoObj >= 0) cena[ultimoObj].trajetoria.velocidade = v;
        }
        else if (tipo == "chao")
        {
            int n;
            float tam;
            ss >> n >> tam;
            gerarChao(n, tam);
        }
    }
    arq.close();

    // Inicia automaticamente as trajetorias predefinidas
    for (size_t i = 0; i < cena.size(); i++)
        if (!cena[i].trajetoria.estaVazia())
            cena[i].trajetoria.iniciar();

    return !cena.empty();
}


// --------------------------------------------------------------------------
// Gravacao do estado atual da cena no arquivo de configuracao
// --------------------------------------------------------------------------
void salvarCena(const string& caminho)
{
    ofstream o(caminho.c_str());
    if (!o.is_open())
    {
        cerr << "Nao foi possivel gravar o arquivo de cena: " << caminho << endl;
        return;
    }

    o << "# Arquivo de configuracao de cena\n";
    o << "# camera  px py pz  yaw pitch  fov near far\n";
    o << "camera " << camera.posicao.x << " " << camera.posicao.y << " " << camera.posicao.z
      << " " << camera.yaw << " " << camera.pitch
      << " " << fovCamera << " " << znear << " " << zfar << "\n\n";

    o << "# luz  indice  intensidade  ligada\n";
    for (int i = 0; i < 3; i++)
        o << "luz " << i << " " << intensidadeLuz[i] << " " << (lucesAtivas[i] ? 1 : 0) << "\n";
    o << "\n";

    o << "# objeto  caminho  px py pz  ex ey ez anguloGraus  escala\n";
    for (size_t i = 0; i < cena.size(); i++)
    {
        Malha& m = cena[i];
        o << "objeto " << m.caminho << " "
          << m.pos.x << " " << m.pos.y << " " << m.pos.z << " "
          << m.eixo.x << " " << m.eixo.y << " " << m.eixo.z << " "
          << glm::degrees(m.angulo) << " " << m.esc.x << "\n";

        for (size_t j = 0; j < m.trajetoria.controle.size(); j++)
        {
            glm::vec3 p = m.trajetoria.controle[j];
            o << "traj " << p.x << " " << p.y << " " << p.z << "\n";
        }
        if (!m.trajetoria.controle.empty())
            o << "veltraj " << m.trajetoria.velocidade << "\n";
        o << "\n";
    }

    o.close();
    cout << "Cena gravada em " << caminho << endl;
}
