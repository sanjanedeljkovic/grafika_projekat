#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/model.h>

#include <iostream>

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

void mouse_callback(GLFWwindow *window, double xpos, double ypos);

void scroll_callback(GLFWwindow *window, double xoffset, double yoffset);

void processInput(GLFWwindow *window);

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

unsigned int loadTexture(char const * path);

unsigned int loadCubemap(vector<std::string> faces);

void renderQuad();

// promenljive
const unsigned int SCR_WIDTH = 900;
const unsigned int SCR_HEIGHT = 667;
bool bloom = true;
float exposure = 0.9f;

// kamera
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct PointLight {
    glm::vec3 position;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;

    float constant;
    float linear;
    float quadratic;
};

struct ProgramState {
    glm::vec3 clearColor = glm::vec3(0);
    bool ImGuiEnabled = false;
    Camera camera;
    bool CameraMouseMovementUpdateEnabled = true;
    bool PokemonAttackMode = false;
    glm::vec3 pokemonPosition = glm::vec3(1.0f);
    float pokemonScale = 1.0f;
    PointLight pointLight;
    ProgramState()
            : camera(glm::vec3(-1.0f, 0.0f, 3.0f)) {}

    void SaveToFile(std::string filename);

    void LoadFromFile(std::string filename);
};

void ProgramState::SaveToFile(std::string filename) {
    std::ofstream out(filename);
    out << clearColor.r << '\n'
        << clearColor.g << '\n'
        << clearColor.b << '\n'
        << ImGuiEnabled << '\n'
        << camera.Position.x << '\n'
        << camera.Position.y << '\n'
        << camera.Position.z << '\n'
        << camera.Front.x << '\n'
        << camera.Front.y << '\n'
        << camera.Front.z << '\n';
}

void ProgramState::LoadFromFile(std::string filename) {
    std::ifstream in(filename);
    if (in) {
        in >> clearColor.r
           >> clearColor.g
           >> clearColor.b
           >> ImGuiEnabled
           >> camera.Position.x
           >> camera.Position.y
           >> camera.Position.z
           >> camera.Front.x
           >> camera.Front.y
           >> camera.Front.z;
    }
}

ProgramState *programState;

void DrawImGui(ProgramState *programState);

int main() {

    // glfw: inicijalizacija
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow *window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetKeyCallback(window, key_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    //stbi_set_flip_vertically_on_load(true);

    programState = new ProgramState;
    programState->LoadFromFile("resources/program_state.txt");
    if (programState->ImGuiEnabled) {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    // ImGui: inicijalizacija
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    (void) io;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    // postavljanje global opengl state (depth testing / face culling / advanced lightning)
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // buildovanje shadera
    Shader ourShader("resources/shaders/2.model_lighting.vs", "resources/shaders/2.model_lighting.fs");
    Shader smallShader("resources/shaders/2.model_lighting.vs", "resources/shaders/2.model_lighting.fs");

    Shader skyboxShader("resources/shaders/skybox.vs", "resources/shaders/skybox.fs");
    Shader surfaceShader("resources/shaders/surface_lightning.vs", "resources/shaders/surface_lightning.fs");
    Shader pinkShader("resources/shaders/pink_light.vs", "resources/shaders/pink_light.fs");
    Shader yellowShader("resources/shaders/yellow_light.vs", "resources/shaders/yellow_light.fs");
    Shader cloudShader("resources/shaders/cloud_blending.vs", "resources/shaders/cloud_blending.fs");

    Shader shaderBlur("resources/shaders/blur.vs", "resources/shaders/blur.fs");
    Shader shaderBloomFinal("resources/shaders/bloom_final.vs", "resources/shaders/bloom_final.fs");

    // ucitavanje modela
    Model ourModel("resources/objects/chin/Resultado.obj");
    ourModel.SetShaderTextureNamePrefix("material.");

    Model smallModel("resources/objects/chin2/Resultado.obj");
    smallModel.SetShaderTextureNamePrefix("material.");

    // pointLight konstante
    PointLight& pointLight = programState->pointLight;
    pointLight.constant = 1.0f;
    pointLight.linear = 0.02f;
    pointLight.quadratic = 0.1;

    // KOCKA vertex (surface, yellow, pink)
    float vertices[] = {
            // pozicije                         // koordinate tekstura          // normale
            -0.5f, -0.5f, -0.5f   ,   0.0f, 0.0f,          0.0f, 0.0f, -1.0f,
            0.5f,  0.5f, -0.5f,     1.0f, 1.0f,         0.0f, 0.0f, -1.0f,
            0.5f, -0.5f, -0.5f,   1.0f, 0.0f,         0.0f, 0.0f, -1.0f,
            0.5f,  0.5f, -0.5f,   1.0f, 1.0f,         0.0f, 0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 0.0f,         0.0f, 0.0f, -1.0f,
            -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,         0.0f, 0.0f, -1.0f,


            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,     0.0f, 0.0f, 1.0f,
            0.5f, -0.5f,  0.5f,  1.0f, 0.0f,      0.0f, 0.0f, 1.0f,
            0.5f,  0.5f,  0.5f,  1.0f, 1.0f,      0.0f, 0.0f, 1.0f,
            0.5f,  0.5f,  0.5f,  1.0f, 1.0f,      0.0f, 0.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f, 1.0f,     0.0f, 0.0f, 1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,     0.0f, 0.0f, 1.0f,

            -0.5f,  0.5f,  0.5f,  1.0f, 0.0f,    -1.0f, 0.0f, 0.0f,
            -0.5f,  0.5f, -0.5f,  1.0f, 1.0f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, -1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f, -1.0f, 0.0f, 0.0f,
            -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, -1.0f, 0.0f, 0.0f,

            0.5f,  0.5f,  0.5f,  1.0f, 0.0f,   1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f,
            0.5f,  0.5f, -0.5f,  1.0f, 1.0f,   1.0f, 0.0f, 0.0f,
            0.5f, -0.5f, -0.5f,  0.0f, 1.0f,   1.0f, 0.0f, 0.0f,
            0.5f,  0.5f,  0.5f,  1.0f, 0.0f,   1.0f, 0.0f, 0.0f,
            0.5f, -0.5f,  0.5f,  0.0f, 0.0f,   1.0f, 0.0f, 0.0f,

            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,
            0.5f, -0.5f, -0.5f,  1.0f, 1.0f,  0.0f, -1.0f, 0.0f,
            0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
            0.5f, -0.5f,  0.5f,  1.0f, 0.0f,  0.0f, -1.0f, 0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, 0.0f,  0.0f, -1.0f, 0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f,  0.0f, -1.0f, 0.0f,

            -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,    0.0f, 1.0f, 0.0f,
            0.5f,  0.5f,  0.5f,  1.0f, 0.0f,    0.0f, 1.0f, 0.0f,
            0.5f,  0.5f, -0.5f,  1.0f, 1.0f,    0.0f, 1.0f, 0.0f,
            0.5f,  0.5f,  0.5f,  1.0f, 0.0f,    0.0f, 1.0f, 0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f, 1.0f,    0.0f, 1.0f, 0.0f,
            -0.5f,  0.5f,  0.5f,  0.0f, 0.0f,    0.0f, 1.0f, 0.0f

    };

    unsigned int VBO_surface, VAO_surface;
    glGenVertexArrays(1, &VAO_surface);
    glGenBuffers(1, &VBO_surface);

    glBindVertexArray(VAO_surface);
    glBindBuffer(GL_ARRAY_BUFFER, VBO_surface);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8* sizeof(float), (void*)(5*sizeof(float)));
    glEnableVertexAttribArray(2);

    unsigned int surface_texture = loadTexture(FileSystem::getPath("resources/textures/zuto.jpg").c_str());

    surfaceShader.use();
    surfaceShader.setInt("texture_diffuse1", 0);

    // OBLAK vertex (cloud) (blending: discarding fragments)
    float planeVertices[] = {
            // pozicije                           // koordinate tekstura
            5.0f, -0.5f,  5.0f,        2.0f, 0.0f,
            -5.0f, -0.5f,  5.0f,       0.0f, 0.0f,
            -5.0f, -0.5f, -5.0f,    0.0f, 2.0f,

            5.0f, -0.5f,  5.0f,    2.0f, 0.0f,
            -5.0f, -0.5f, -5.0f,   0.0f, 2.0f,
            5.0f, -0.5f, -5.0f,    2.0f, 2.0f
    };
    float transparentVertices[] = {
            // pozicije                            // koordinate tekstura
            0.0f,  0.5f,  0.0f,        0.0f,  0.0f,
            0.0f, -0.5f,  0.0f,        0.0f,  1.0f,
            1.0f, -0.5f,  0.0f,     1.0f,  1.0f,

            0.0f,  0.5f,  0.0f,     0.0f,  0.0f,
            1.0f, -0.5f,  0.0f,     1.0f,  1.0f,
            1.0f,  0.5f,  0.0f,     1.0f,  0.0f
    };

    unsigned int planeVAO, planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), &planeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));

    unsigned int transparentVAO, transparentVBO;
    glGenVertexArrays(1, &transparentVAO);
    glGenBuffers(1, &transparentVBO);
    glBindVertexArray(transparentVAO);
    glBindBuffer(GL_ARRAY_BUFFER, transparentVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(transparentVertices), transparentVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glBindVertexArray(0);

    unsigned int transparentTexture = loadTexture(FileSystem::getPath("resources/textures/roze4.png").c_str());

    cloudShader.use();
    cloudShader.setInt("texture1", 0);


    // POZICIONIRANJA

    // pozicije svetlecih kocki po sceni
    glm::vec3 cubePositions[] = {
            glm::vec3( 20.0f,  0.0f,  0.0f),
            glm::vec3( 30.0f,  -5.0f, -5.0f),
            glm::vec3(15.5f, 4.2f, 7.5f),
            glm::vec3(18.8f, 7.0f, 20.3f),
            glm::vec3( 12.4f, -2.4f, -3.5f),
            glm::vec3(3.7f,  -0.8f, 17.5f),
            glm::vec3( 33.3f, -2.0f, 12.5f),
            glm::vec3( 21.5f,  -4.0f, -2.5f),
            glm::vec3( 1.5f,  -6.2f, -1.5f),
            glm::vec3(6.3f,  -7.0f, -6.5f)
    };

    // pozicije oblaka
    vector<glm::vec3> cloud_positions
            {
                    glm::vec3(25.0f, -2.0f, -2.48f), // uz mali
                    glm::vec3( 5.0f, -3.0f, 9.7f),
                    glm::vec3(-3.3f, 1.0f, 11.3f),
                    glm::vec3( 30.5f, 2.0f, 4.51f), // uz mali
                    glm::vec3(0.5f, -1.0f, 14.6f)
            };

    // postavljanje skybox
    vector<std::string> faces
            {
                    FileSystem::getPath("resources/textures/front.jpg"),
                    FileSystem::getPath("resources/textures/back.jpg"),
                    FileSystem::getPath("resources/textures/up.jpg"),
                    FileSystem::getPath("resources/textures/down.jpg"),
                    FileSystem::getPath("resources/textures/right.jpg"),
                    FileSystem::getPath("resources/textures/left.jpg")
            };

    float skyboxVertices[] = {
            // positions
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,

            -1.0f, -1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,

            -1.0f,  1.0f, -1.0f,
            1.0f,  1.0f, -1.0f,
            1.0f,  1.0f,  1.0f,
            1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f,  1.0f,
            -1.0f,  1.0f, -1.0f,

            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f, -1.0f,
            1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
            1.0f, -1.0f,  1.0f
    };

    unsigned int skyboxVAO, skyboxVBO;
    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    unsigned int cubemapTexture = loadCubemap(faces);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);


    // frame buffers: hdr & bloom
    unsigned int hdrFBO;
    glGenFramebuffers(1, &hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
    // 2 color buffers
    unsigned int colorBuffers[2];
    glGenTextures(2, colorBuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindTexture(GL_TEXTURE_2D, colorBuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, colorBuffers[i], 0);
    }
    // depth buffer (renderbuffer)
    unsigned int rboDepth;
    glGenRenderbuffers(1, &rboDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, rboDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, SCR_WIDTH, SCR_HEIGHT);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rboDepth);

    unsigned int attachments[2] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
    glDrawBuffers(2, attachments);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cout << "Framebuffer not complete!" << std::endl;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // ping-pong-framebuffer
    unsigned int pingpongFBO[2];
    unsigned int pingpongColorbuffers[2];
    glGenFramebuffers(2, pingpongFBO);
    glGenTextures(2, pingpongColorbuffers);
    for (unsigned int i = 0; i < 2; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pingpongColorbuffers[i], 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            std::cout << "Framebuffer not complete!" << std::endl;
    }

    shaderBlur.use();
    shaderBlur.setInt("image", 0);
    shaderBloomFinal.use();
    shaderBloomFinal.setInt("scene", 0);
    shaderBloomFinal.setInt("bloomBlur", 1);

    while (!glfwWindowShouldClose(window)) {

        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        processInput(window);

        // render
        // ------------------------------------------------------------------------------------------------------------------------
        glClearColor(programState->clearColor.r, programState->clearColor.g, programState->clearColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, hdrFBO);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ------------------------------------------------------------------------------------------------------------------------
        // PLAVI MODEL
        // ------------------------------------------------------------------------------------------------------------------------

        ourShader.use();

        // point light (0 - pink, 1 - yellow)
        pointLight.position = glm::vec3(5.0f, 11.0f+sin(glfwGetTime()*3.0f) * (-2.5f), 6.0f+cos(glfwGetTime()*3.0f)*1.0f);
        ourShader.setVec3("pointLight[0].position", pointLight.position);
        ourShader.setVec3("pointLight[0].ambient", 1.0, 0.0, 1.0);
        ourShader.setVec3("pointLight[0].diffuse", 1.0, 0.0, 1.0);
        ourShader.setVec3("pointLight[0].specular", 1.0, 1.0, 1.0);
        ourShader.setFloat("pointLight[0].constant", pointLight.constant);
        ourShader.setFloat("pointLight[0].linear", pointLight.linear);
        ourShader.setFloat("pointLight[0].quadratic", pointLight.quadratic);
        ourShader.setVec3("viewPosition", programState->camera.Position);
        ourShader.setFloat("material.shininess", 256.0f);

        pointLight.position = glm::vec3 (5.0f, 11.0f+sin(glfwGetTime()*3.0f) * 2.5f, -4.0f+cos(glfwGetTime()*3.0f)*1.0f);
        ourShader.setVec3("pointLight[1].position", pointLight.position);
        ourShader.setVec3("pointLight[1].ambient", 0.7, 0.7, 0.0);
        ourShader.setVec3("pointLight[1].diffuse", 1.0, 1.0, 0.0);
        ourShader.setVec3("pointLight[1].specular", 1.0, 1.0, 1.0);
        ourShader.setFloat("pointLight[1].constant", pointLight.constant);
        ourShader.setFloat("pointLight[1].linear", pointLight.linear);
        ourShader.setFloat("pointLight[1].quadratic", pointLight.quadratic);

        // directional light
        ourShader.setVec3("dirLight.direction", 0.0f, -5.0f, -15.0f);
        ourShader.setVec3("dirLight.ambient", 0.4f, 0.4f, 0.1f);
        ourShader.setVec3("dirLight.diffuse", 0.2f, 0.2f, 0.1f);
        ourShader.setVec3("dirLight.specular", 1.0f, 1.0f, 1.0f);

        // spotlight
        ourShader.setVec3("spotLight.position", programState->camera.Position);
        ourShader.setVec3("spotLight.direction", programState->camera.Front);
        ourShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
        ourShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
        ourShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
        ourShader.setFloat("spotLight.constant", 1.0f);
        ourShader.setFloat("spotLight.linear", 0.022);
        ourShader.setFloat("spotLight.quadratic", 0.0019);
        ourShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(10.0f)));
        ourShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));

        // matrice transformacija: view, projection
        glm::mat4 projection = glm::perspective(glm::radians(programState->camera.Zoom),
                                                (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.3f, 500.0f);
        glm::mat4 view = programState->camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        // model matrica i render
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model,programState->pokemonPosition);
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0, 1.0, 0.0));
        model = glm::scale(model, glm::vec3(programState->pokemonScale));
        ourShader.setMat4("model", model);

        ourModel.Draw(ourShader);


        // ------------------------------------------------------------------------------------------------------------------------
        // LJUBICASTI MODEL
        // ------------------------------------------------------------------------------------------------------------------------

        smallShader.use();

        // point light (0 - pink, 1 - yellow)
        pointLight.position = glm::vec3(30.0f, 11.0f+sin(glfwGetTime()*3.0f) * (-1.5f), 6.0f+cos(glfwGetTime()*3.0f)*1.0f-2.0);
        smallShader.setVec3("pointLight[0].position", pointLight.position);
        smallShader.setVec3("pointLight[0].ambient", 1.0, 0.0, 1.0);
        smallShader.setVec3("pointLight[0].diffuse", 1.0, 0.0, 1.0);
        smallShader.setVec3("pointLight[0].specular", 1.0, 1.0, 1.0);
        smallShader.setFloat("pointLight[0].constant", pointLight.constant);
        smallShader.setFloat("pointLight[0].linear", pointLight.linear);
        smallShader.setFloat("pointLight[0].quadratic", pointLight.quadratic);
        smallShader.setVec3("viewPosition", programState->camera.Position);
        smallShader.setFloat("material.shininess", 256.0f);

        pointLight.position = glm::vec3 (30.0f, 11.0f+sin(glfwGetTime()*3.0f) * 1.5f, -4.0f+cos(glfwGetTime()*3.0f)*1.0f+2.0);
        smallShader.setVec3("pointLight[1].position", pointLight.position);
        smallShader.setVec3("pointLight[1].ambient", 1.0, 1.0, 0.0);
        smallShader.setVec3("pointLight[1].diffuse", 1.0, 1.0, 0.0);
        smallShader.setVec3("pointLight[1].specular", 1.0, 1.0, 1.0);
        smallShader.setFloat("pointLight[1].constant", pointLight.constant);
        smallShader.setFloat("pointLight[1].linear", pointLight.linear);
        smallShader.setFloat("pointLight[1].quadratic", pointLight.quadratic);

        // directional light
        smallShader.setVec3("dirLight.direction", 0.0f, -5.0f, -15.0f);
        smallShader.setVec3("dirLight.ambient", 0.3f, 0.4f, 0.1f);
        smallShader.setVec3("dirLight.diffuse", 0.1f, 0.2f, 0.1f);
        smallShader.setVec3("dirLight.specular", 1.0f, 1.0f, 1.0f);

        // spotlight
        smallShader.setVec3("spotLight.position", programState->camera.Position);
        smallShader.setVec3("spotLight.direction", programState->camera.Front);
        smallShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
        smallShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
        smallShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
        smallShader.setFloat("spotLight.constant", 1.0f);
        smallShader.setFloat("spotLight.linear", 0.022);
        smallShader.setFloat("spotLight.quadratic", 0.0019);
        smallShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(10.0f)));
        smallShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));

        // matrice transformacija: view, projection
        smallShader.setMat4("projection", projection);
        smallShader.setMat4("view", view);

        // model matrica i render
        glm::mat4 model1 = glm::mat4(1.0f);
        model1 = glm::translate(model1,programState->pokemonPosition);
        model1 = glm::translate(model1, glm::vec3(30.0, 0.0, 0.0));
        model1 = glm::rotate(model1, glm::radians(270.0f), glm::vec3(0.0, 1.0, 0.0));
        model1 = glm::scale(model1, glm::vec3(programState->pokemonScale));
        model1 = glm::scale(model1, glm::vec3(0.5, 0.5, 0.5));
        smallShader.setMat4("model", model1);

        smallModel.Draw(smallShader);

        // ------------------------------------------------------------------------------------------------------------------------
        // KOCKA: SURFACE
        // ------------------------------------------------------------------------------------------------------------------------

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, surface_texture);

        surfaceShader.use();

        // directional light
        surfaceShader.setVec3("dirLight.direction", 0.0f, -5.0f, -15.0f);
        surfaceShader.setVec3("dirLight.ambient", 0.6, 0.4f, 0.1f);
        surfaceShader.setVec3("dirLight.diffuse", 0.7f, 0.7f, 0.7f);
        surfaceShader.setVec3("dirLight.specular", 1.0f, 1.0f, 1.0f);
        surfaceShader.setVec3("viewPosition", programState->camera.Position);
        surfaceShader.setFloat("material.shininess", 126.0f);

        // spotlight
        surfaceShader.setVec3("spotLight.position", programState->camera.Position);
        surfaceShader.setVec3("spotLight.direction", programState->camera.Front);
        surfaceShader.setVec3("spotLight.ambient", 0.0f, 0.0f, 0.0f);
        surfaceShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
        surfaceShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
        surfaceShader.setFloat("spotLight.constant", 1.0f);
        surfaceShader.setFloat("spotLight.linear", 0.022);
        surfaceShader.setFloat("spotLight.quadratic", 0.0019);
        surfaceShader.setFloat("spotLight.cutOff", glm::cos(glm::radians(10.0f)));
        surfaceShader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(15.0f)));

        // matrice transformacija: view, projection
        surfaceShader.setMat4("projection", projection);
        surfaceShader.setMat4("view", view);

        // model matrica i render kocke za plavi model
        glm::mat4 surface_model = glm::mat4(1.0f);
        surface_model = glm::translate(surface_model, glm::vec3(1.0f, -0.9f, 1.5f));
        surface_model = glm::scale(surface_model, glm::vec3(8.0, 4.0, 8.0));
        surfaceShader.setMat4("model", surface_model);

        glBindVertexArray(VAO_surface);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // model matrica i render kocke za ljubicasti model
        surface_model = glm::translate(surface_model, glm::vec3(3.72f, 0.25f, -0.07f));
        surface_model = glm::scale(surface_model, glm::vec3(0.5, 0.5, 0.5));
        surfaceShader.setMat4("model", surface_model);
        glBindVertexArray(VAO_surface);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // ------------------------------------------------------------------------------------------------------------------------
        // KOCKA: PINK LIGHT
        // ------------------------------------------------------------------------------------------------------------------------

        pinkShader.use();

        // matrice transformacija: view, projection
        pinkShader.setMat4("projection", projection);
        pinkShader.setMat4("view", view);

        // model matrica i render kocke za plavi model
        glm::mat4 pink_model = glm::mat4(1.0f);
        pink_model = glm::translate(pink_model, glm::vec3(5.0f, 11.0f+sin(glfwGetTime()*3.0f) * (-2.5f), 6.0f+cos(glfwGetTime()*3.0f)*1.0f));
        pink_model = glm::scale(pink_model, glm::vec3(0.6, 0.6, 0.6));
        pinkShader.setMat4("model", pink_model);

        glBindVertexArray(VAO_surface);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // model matrica i render kocke za ljubicasti model
        pink_model = glm::translate(pink_model, glm::vec3(41.0f, -5.0, -6.0));
        pink_model = glm::scale(pink_model, glm::vec3(0.3, 0.3, 0.3));
        yellowShader.setMat4("model", pink_model);
        glBindVertexArray(VAO_surface);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // ------------------------------------------------------------------------------------------------------------------------
        // KOCKA: YELLOW LIGHT
        // ------------------------------------------------------------------------------------------------------------------------

        yellowShader.use();

        // matrice transformacija: view, projection
        yellowShader.setMat4("projection", projection);
        yellowShader.setMat4("view", view);

        // model matrica i render kocke za plavi model
        glm::mat4 yellow_model = glm::mat4(1.0f);
        yellow_model = glm::translate(yellow_model, glm::vec3(5.0f, 11.0f+sin(glfwGetTime()*3.0f) * 2.5f, -4.0f+cos(glfwGetTime()*3.0f)*1.0f));
        yellow_model = glm::scale(yellow_model, glm::vec3(0.6, 0.6, 0.6));
        yellowShader.setMat4("model", yellow_model);

        glBindVertexArray(VAO_surface);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // model matrica i render kocke za ljubicasti model
        yellow_model = glm::translate(yellow_model, glm::vec3(41.0f, -5.0, 6.0));
        yellow_model = glm::scale(yellow_model, glm::vec3(0.3, 0.3, 0.3));
        yellowShader.setMat4("model", yellow_model);
        glBindVertexArray(VAO_surface);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        // ------------------------------------------------------------------------------------------------------------------------
        // KOCKA: YELLOW LIGHT po sceni
        // ------------------------------------------------------------------------------------------------------------------------

        for (unsigned int i = 0; i < 10; i++)
        {
            glm::mat4 yellow_model = glm::mat4(1.0f);
            yellow_model = glm::translate(yellow_model, cubePositions[i]);
            yellow_model = glm::scale(yellow_model, glm::vec3(0.3, 0.3, 0.3));
            float angle = 20.0f * i;
            model = glm::rotate(yellow_model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
            yellowShader.setMat4("model", yellow_model);

            glDrawArrays(GL_TRIANGLES, 0, 36);
        }


        // ------------------------------------------------------------------------------------------------------------------------
        // OBLAK
        // ------------------------------------------------------------------------------------------------------------------------

        cloudShader.use();

        cloudShader.setMat4("projection", projection);
        cloudShader.setMat4("view", view);
        glm::mat4 cloud_model = glm::mat4(1.0f);
        glBindVertexArray(transparentVAO);
        glBindTexture(GL_TEXTURE_2D, transparentTexture);

        // disable face culling kako bi se renderovale obe strane
        glDisable(GL_CULL_FACE);
        for (unsigned int i = 0; i < cloud_positions.size(); i++)
        {
            cloud_model = glm::mat4(1.0f);
            cloud_model = glm::translate(cloud_model, cloud_positions[i]);
            cloud_model = glm::scale(cloud_model, glm::vec3(7.0+i,  7.0+i, 7.0+i));
            cloudShader.setMat4("model", cloud_model);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
        glEnable(GL_CULL_FACE);


        // ------------------------------------------------------------------------------------------------------------------------
        // SKYBOX
        // ------------------------------------------------------------------------------------------------------------------------

        glDepthFunc(GL_LEQUAL);

        skyboxShader.use();
        // matrice transformacije: view, projection ( + uklanjanje translacije iz matrice pogleda)
        view = glm::mat4(glm::mat3(programState->camera.GetViewMatrix()));
        skyboxShader.setMat4("view", view);
        skyboxShader.setMat4("projection", projection);

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, cubemapTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS);

        // ------------------------------------------------------------------------------------------------------------------------
        // HDR & BLOOM
        // ------------------------------------------------------------------------------------------------------------------------

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        bool horizontal = true, first_iteration = true;
        unsigned int amount = 10;

        shaderBlur.use();

        // guassian blur za svetle fragmente
        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFBO[horizontal]);
            shaderBlur.setInt("horizontal", horizontal);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? colorBuffers[1] : pingpongColorbuffers[!horizontal]);
            renderQuad();
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }

        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // renderovanje floating point color buffera, tonemap HDR boja
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        shaderBloomFinal.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, colorBuffers[0]);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongColorbuffers[!horizontal]);
        shaderBloomFinal.setInt("bloom", bloom);
        shaderBloomFinal.setFloat("exposure", exposure);
        renderQuad();

        //glBindVertexArray(0);

        // ImGui
        if (programState->ImGuiEnabled)
            DrawImGui(programState);



        // glfw: swap buffers & poll IO events
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    programState->SaveToFile("resources/program_state.txt");
    delete programState;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // glfw: deaktiviranje i ciscenje:

    glDeleteVertexArrays(1, &VAO_surface);
    glDeleteVertexArrays(1, &planeVAO);
    glDeleteVertexArrays(1, &transparentVAO);
    glDeleteVertexArrays(1, &skyboxVAO);

    glDeleteBuffers(1, &VBO_surface);
    glDeleteBuffers(1, &planeVBO);
    glDeleteBuffers(1, &transparentVBO);
    glDeleteBuffers(1, &skyboxVBO);

    glDeleteFramebuffers(1, &hdrFBO);
    glDeleteFramebuffers(1, &pingpongFBO[0]);
    glDeleteFramebuffers(1, &pingpongFBO[2]);

    glDeleteTextures(1, &surface_texture);
    glDeleteTextures(1, &transparentTexture);
    glDeleteTextures(1, &cubemapTexture);

    glfwTerminate();
    return 0;
}

// process all input: poziva se prilikom dodira na releventne tipke

void processInput(GLFWwindow *window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(FORWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(BACKWARD, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(LEFT, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        programState->camera.ProcessKeyboard(RIGHT, deltaTime);
}

// glfw: poziva se prilikom promene velicine prozora
void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}

// glfw: poziva se prilikom pomeraja misa
void mouse_callback(GLFWwindow *window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

    lastX = xpos;
    lastY = ypos;

    if (programState->CameraMouseMovementUpdateEnabled)
        programState->camera.ProcessMouseMovement(xoffset, yoffset);
}

// glfw: poziva se prilikom skrolovanja misem
void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {
    programState->camera.ProcessMouseScroll(yoffset);
}

// naredba za crtanje Guia
void DrawImGui(ProgramState *programState) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();


    {
        static float f = 0.0f;
        ImGui::Begin("Chinchou!");
        ImGui::SliderFloat("Sliding speed", &f, 0.0, 1.0);

        ImGui::Text("Press B for attack mode!");
        ImGui::DragFloat("Calmness level", &programState->pointLight.constant, 0.05, 0.05, 1.0);
        ImGui::Text("Press R for peace :)");

        ImGui::End();
    }

    {
        ImGui::Begin("Camera info");
        const Camera& c = programState->camera;
        ImGui::Text("Camera position: (%f, %f, %f)", c.Position.x, c.Position.y, c.Position.z);
        ImGui::Text("(Yaw, Pitch): (%f, %f)", c.Yaw, c.Pitch);
        ImGui::Text("Camera front: (%f, %f, %f)", c.Front.x, c.Front.y, c.Front.z);
        ImGui::Checkbox("Camera mouse update", &programState->CameraMouseMovementUpdateEnabled);
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// glfw: poziva se prilikom dodirivanja tipki
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_U && action == GLFW_PRESS) {
        programState->ImGuiEnabled = !programState->ImGuiEnabled;
        if (programState->ImGuiEnabled) {
            programState->CameraMouseMovementUpdateEnabled = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }

    if (key == GLFW_KEY_B && action == GLFW_PRESS) {
            programState->pointLight.linear = 0.0f;
            programState->pointLight.quadratic = 0.0f;
        }

    if (key == GLFW_KEY_R && action == GLFW_PRESS) {
           programState->pointLight.constant = 1.0f;
           programState->pointLight.linear = 0.02f;
           programState->pointLight.quadratic = 0.1f;
    }

}

// ucitavanje teksture + prilagodjeno za HDR
unsigned int loadTexture(char const * path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char *data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        GLenum iternal;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3) {
            format = GL_RGB;
            iternal = GL_SRGB;
        }
        else if (nrComponents == 4) {
            iternal = GL_SRGB_ALPHA;
            format = GL_RGBA;
        }

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, iternal, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT); // for this tutorial: use GL_CLAMP_TO_EDGE to prevent semi-transparent borders. Due to interpolation it takes texels from next repeat
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, format == GL_RGBA ? GL_CLAMP_TO_EDGE : GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}

// ucitavanje tekstura za skybox
unsigned int loadCubemap(vector<std::string> faces)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load((faces[i].c_str()), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else
        {
            std::cout << "Cubemap texture failed to load at path: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}



// renderovanje za bloom
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
    if (quadVAO == 0)
    {
        float quadVertices[] = {
                // pozicije                    // koordinate tekstura
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
        };


        glGenVertexArrays(1, &quadVAO);
        glGenBuffers(1, &quadVBO);
        glBindVertexArray(quadVAO);
        glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    }
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}