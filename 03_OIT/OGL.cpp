#include <windows.h>
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <iostream>

#include <GL/glew.h>
#include <gl/GL.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "WindowManager.h"
#include "Logger.h"
#include "Timer.h"
#include "camera.h"
#include "Shader.h"
#include "model.h"



//*** Globle Function Declarations ***
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
unsigned int loadTexture(const char* path);
unsigned int loadTexture(const char* path, bool gammaCorrection);
void renderCube();
void renderQuad();
glm::mat4 calculate_model_matrix(const glm::vec3& position, const glm::vec3& rotation = glm::vec3(0.0f), const glm::vec3& scale = glm::vec3(1.0f));


//*** Global Variable Declaration ***
WindowManager* pWindow = NULL;
Camera* camera = NULL;
float lastX = WindowManager::SCR_WIDTH / 2.0f;
float lastY = WindowManager::SCR_HEIGHT / 2.0f;
bool firstMouse = true;
// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;



///==================== OpenGL Variables =======================///
GLuint vaoCube = 0;
GLuint vbo_position_cube = 0;
GLuint vbo_normal_cube = 0;

bool bloom = true;
bool bloomKeyPressed = false;
float exposure = 1.0f;




int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int iCmdShow)
{
	pWindow = new WindowManager();
	MSG msg = { 0 };
	camera = new Camera();

	Logger::Init();

	TIMER_INIT("Window");
	pWindow->initialize();
	TIMER_END(); 
	LOG_INFO("Window Initialized in %.6f seconds", TIMER_GET("Window"));



	///======================== OpenGL INIT ==============================///
	Shader solidShader("shaders/solid.vs", "shaders/solid.fs");
	Shader transparentShader("shaders/transparent.vs", "shaders/transparent.fs");
	Shader compositeShader("shaders/composite.vs", "shaders/composite.fs");
	Shader screenShader("shaders/screen.vs", "shaders/screen.fs");

	// set up vertex data (and buffer(s)) and configure vertex attributes
	// ------------------------------------------------------------------
	float quadVertices[] = {
		// positions		// uv
		-1.0f, -1.0f, 0.0f,	0.0f, 0.0f,
		 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,

		 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
		-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
		-1.0f, -1.0f, 0.0f, 0.0f, 0.0f
	};

	// quad VAO
	unsigned int quadVAO, quadVBO;
	glGenVertexArrays(1, &quadVAO);
	glGenBuffers(1, &quadVBO);
	glBindVertexArray(quadVAO);
	glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glBindVertexArray(0);

	// set up framebuffers and their texture attachments
	// ------------------------------------------------------------------
	unsigned int opaqueFBO, transparentFBO;
	glGenFramebuffers(1, &opaqueFBO);
	glGenFramebuffers(1, &transparentFBO);

	// set up attachments for opaque framebuffer
	unsigned int opaqueTexture;
	glGenTextures(1, &opaqueTexture);
	glBindTexture(GL_TEXTURE_2D, opaqueTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WindowManager::SCR_WIDTH, WindowManager::SCR_HEIGHT, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	unsigned int depthTexture;
	glGenTextures(1, &depthTexture);
	glBindTexture(GL_TEXTURE_2D, depthTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, WindowManager::SCR_WIDTH, WindowManager::SCR_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, opaqueFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, opaqueTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Opaque framebuffer is not complete!" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// set up attachments for transparent framebuffer
	unsigned int accumTexture;
	glGenTextures(1, &accumTexture);
	glBindTexture(GL_TEXTURE_2D, accumTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, WindowManager::SCR_WIDTH, WindowManager::SCR_HEIGHT, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	unsigned int revealTexture;
	glGenTextures(1, &revealTexture);
	glBindTexture(GL_TEXTURE_2D, revealTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, WindowManager::SCR_WIDTH, WindowManager::SCR_HEIGHT, 0, GL_RED, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, transparentFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, accumTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, revealTexture, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0); // opaque framebuffer's depth texture

	const GLenum transparentDrawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	glDrawBuffers(2, transparentDrawBuffers);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		std::cout << "ERROR::FRAMEBUFFER:: Transparent framebuffer is not complete!" << std::endl;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// set up transformation matrices
	// ------------------------------------------------------------------
	glm::mat4 redModelMat = calculate_model_matrix(glm::vec3(0.0f, 0.0f, 1.0f));
	glm::mat4 greenModelMat = calculate_model_matrix(glm::vec3(0.0f, 0.0f, 0.0f));
	glm::mat4 blueModelMat = calculate_model_matrix(glm::vec3(0.0f, 0.0f, 2.0f));

	// set up intermediate variables
	// ------------------------------------------------------------------
	glm::vec4 zeroFillerVec(0.0f);
	glm::vec4 oneFillerVec(1.0f);





	//*** Game LOOP ***
	while (pWindow->isRunning == FALSE)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
				pWindow->isRunning = TRUE;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			///=========================== DISPLAY ==================================//
			// per-frame time logic
			// --------------------
			float currentFrame = static_cast<float>(Timer::getAppRunTime() * 0.05f);
			deltaTime = currentFrame - lastFrame;
			lastFrame = currentFrame;


			// camera matrices
			glm::mat4 projection = glm::perspective(glm::radians(camera->Zoom), (float)WindowManager::SCR_WIDTH / (float)WindowManager::SCR_HEIGHT, 0.1f, 100.0f);
			glm::mat4 view = camera->GetViewMatrix();
			glm::mat4 vp = projection * view;


			glEnable(GL_DEPTH_TEST);
			glDepthFunc(GL_LESS);
			glDepthMask(GL_TRUE);
			glDisable(GL_BLEND);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

			// bind opaque framebuffer to render solid objects
			glBindFramebuffer(GL_FRAMEBUFFER, opaqueFBO);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			// use solid shader
			solidShader.use();

			// draw red quad
			solidShader.setMat4("mvp", vp * redModelMat);
			solidShader.setVec3("color", glm::vec3(1.0f, 0.0f, 0.0f));
			glBindVertexArray(quadVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// draw transparent objects (transparent pass)
			// -----

			// configure render states
			glDepthMask(GL_FALSE);
			glEnable(GL_BLEND);
			glBlendFunci(0, GL_ONE, GL_ONE);
			glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
			glBlendEquation(GL_FUNC_ADD);

			// bind transparent framebuffer to render transparent objects
			glBindFramebuffer(GL_FRAMEBUFFER, transparentFBO);
			glClearBufferfv(GL_COLOR, 0, &zeroFillerVec[0]);
			glClearBufferfv(GL_COLOR, 1, &oneFillerVec[0]);

			// use transparent shader
			transparentShader.use();

			// draw green quad
			transparentShader.setMat4("mvp", vp * greenModelMat);
			transparentShader.setVec4("color", glm::vec4(0.0f, 1.0f, 0.0f, 0.5f));
			glBindVertexArray(quadVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// draw blue quad
			transparentShader.setMat4("mvp", vp * blueModelMat);
			transparentShader.setVec4("color", glm::vec4(0.0f, 0.0f, 1.0f, 0.5f));
			glBindVertexArray(quadVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// draw composite image (composite pass)
			// -----

			// set render states
			glDepthFunc(GL_ALWAYS);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			// bind opaque framebuffer
			glBindFramebuffer(GL_FRAMEBUFFER, opaqueFBO);

			// use composite shader
			compositeShader.use();

			// draw screen quad
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, accumTexture);
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, revealTexture);
			glBindVertexArray(quadVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);

			// draw to backbuffer (final pass)
			// -----

			// set render states
			glDisable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE); // enable depth writes so glClear won't ignore clearing the depth buffer
			glDisable(GL_BLEND);

			// bind backbuffer
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			// use screen shader
			screenShader.use();

			// draw final screen quad
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, opaqueTexture);
			glBindVertexArray(quadVAO);
			glDrawArrays(GL_TRIANGLES, 0, 6);


			pWindow->swapDisplayBuffer();

			///================== UPDATE =======================//
			//angleCube = angleCube + 0.02f;

		}
	}


	return((int)msg.wParam);
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
{
	//*** Function Declaration ***
	void ToggleFullscreen(void);
	void resize(int, int);


	//*** Code ***
	switch (iMsg)
	{
	case WM_SETFOCUS:
		break;

	case WM_KILLFOCUS:
		break;

	case WM_SIZE:
        glViewport(0, 0, LOWORD(lParam), HIWORD(lParam));
		break;

	case WM_ERASEBKGND:
		return(0);

	case WM_MOUSEMOVE:
		{
			float xpos = static_cast<float>(GET_X_LPARAM(lParam));
			float ypos = static_cast<float>(GET_Y_LPARAM(lParam));

			if (firstMouse)
			{
				lastX = xpos;
				lastY = ypos;
				firstMouse = false;
			}

			float xoffset = xpos - lastX;
			float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

			lastX = xpos;
			lastY = ypos;
			camera->ProcessMouseMovement(xoffset, yoffset);
		}
		
		break;

	case WM_KEYDOWN:
		switch (LOWORD(wParam))
		{
		case VK_ESCAPE:
			DestroyWindow(hwnd);
			break;
		}
		break;

	case WM_CHAR:
		switch (LOWORD(wParam))
		{
		case 'F':
		case 'f':
			
			break;

		case 'W':
		case 'w':
			camera->ProcessKeyboard(FORWARD, deltaTime);
			break;

		case 'A':
		case 'a':
			camera->ProcessKeyboard(LEFT, deltaTime);
			break;

		case 'S':
		case 's':
			camera->ProcessKeyboard(BACKWARD, deltaTime);
			break;

		case 'D':
		case 'd':
			camera->ProcessKeyboard(RIGHT, deltaTime);
			break;

		
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hwnd);
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		break;
	}


	return(DefWindowProc(hwnd, iMsg, wParam, lParam));
}


void renderScene(const Shader& shader)
{
	// room cube
	glm::mat4 model = glm::mat4(1.0f);
	model = glm::scale(model, glm::vec3(5.0f));
	shader.setMat4("model", model);
	glDisable(GL_CULL_FACE); // note that we disable culling here since we render 'inside' the cube instead of the usual 'outside' which throws off the normal culling methods.
	shader.setInt("reverse_normals", 1); // A small little hack to invert normals when drawing cube from the inside so lighting still works.
	renderCube();
	shader.setInt("reverse_normals", 0); // and of course disable it
	glEnable(GL_CULL_FACE);
	// cubes
	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(4.0f, -3.5f, 0.0));
	model = glm::scale(model, glm::vec3(0.5f));
	shader.setMat4("model", model);
	renderCube();
	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(2.0f, 3.0f, 1.0));
	model = glm::scale(model, glm::vec3(0.75f));
	shader.setMat4("model", model);
	renderCube();
	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-3.0f, -1.0f, 0.0));
	model = glm::scale(model, glm::vec3(0.5f));
	shader.setMat4("model", model);
	renderCube();
	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-1.5f, 1.0f, 1.5));
	model = glm::scale(model, glm::vec3(0.5f));
	shader.setMat4("model", model);
	renderCube();
	model = glm::mat4(1.0f);
	model = glm::translate(model, glm::vec3(-1.5f, 2.0f, -3.0));
	model = glm::rotate(model, glm::radians(60.0f), glm::normalize(glm::vec3(1.0, 0.0, 1.0)));
	model = glm::scale(model, glm::vec3(0.75f));
	shader.setMat4("model", model);
	renderCube();
}

unsigned int cubeVAO = 0;
unsigned int cubeVBO = 0;
void renderCube()
{
	// initialize (if necessary)
	if (cubeVAO == 0)
	{
		float vertices[] = {
			// back face
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			 1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
			 1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
			-1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
			-1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
			// front face
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			 1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			 1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
			-1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
			-1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
			// left face
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			-1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
			-1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			-1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
			// right face
			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			 1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
			 1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
			 1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
			 1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
			 // bottom face
			 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			  1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
			  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			  1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
			 -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
			 -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
			 // top face
			 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			  1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			  1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
			  1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
			 -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
			 -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
		};
		glGenVertexArrays(1, &cubeVAO);
		glGenBuffers(1, &cubeVBO);
		// fill buffer
		glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
		// link vertex attributes
		glBindVertexArray(cubeVAO);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		glBindVertexArray(0);
	}
	// render Cube
	glBindVertexArray(cubeVAO);
	glDrawArrays(GL_TRIANGLES, 0, 36);
	glBindVertexArray(0);
}

// renderQuad() renders a 1x1 XY quad in NDC
// -----------------------------------------
unsigned int quadVAO = 0;
unsigned int quadVBO;
void renderQuad()
{
	if (quadVAO == 0)
	{
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		// setup plane VAO
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


unsigned int loadTexture(char const* path, bool gammaCorrection)
{
	unsigned int textureID;
	glGenTextures(1, &textureID);

	int width, height, nrComponents;
	unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
	if (data)
	{
		GLenum internalFormat;
		GLenum dataFormat;
		if (nrComponents == 1)
		{
			internalFormat = dataFormat = GL_RED;
		}
		else if (nrComponents == 3)
		{
			internalFormat = gammaCorrection ? GL_SRGB : GL_RGB;
			dataFormat = GL_RGB;
		}
		else if (nrComponents == 4)
		{
			internalFormat = gammaCorrection ? GL_SRGB_ALPHA : GL_RGBA;
			dataFormat = GL_RGBA;
		}

		glBindTexture(GL_TEXTURE_2D, textureID);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
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


glm::mat4 calculate_model_matrix(const glm::vec3& position, const glm::vec3& rotation, const glm::vec3& scale)
{
	glm::mat4 trans = glm::mat4(1.0f);

	trans = glm::translate(trans, position);
	trans = glm::rotate(trans, glm::radians(rotation.x), glm::vec3(1.0, 0.0, 0.0));
	trans = glm::rotate(trans, glm::radians(rotation.y), glm::vec3(0.0, 1.0, 0.0));
	trans = glm::rotate(trans, glm::radians(rotation.z), glm::vec3(0.0, 0.0, 1.0));
	trans = glm::scale(trans, scale);

	return trans;
}
