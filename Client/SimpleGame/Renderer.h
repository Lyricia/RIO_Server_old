#pragma once

#include <fstream>
#include <iostream>

#include "Dependencies\glew.h"
#include "Dependencies\freeglut.h"

#define MAX_TEXTURES 200

class Renderer
{
public:
	Renderer(int windowSizeX, int windowSizeY);
	~Renderer();

	bool IsInitialized();
	void DrawSolidRect(float x, float y, float z, float size, float r, float g, float b, float a, float level);
	void DrawSolidRectXY(float x, float y, float z, float width, float height, float r, float g, float b, float a, float level);
	void DrawSolidRectGauge(float x, float y, float z, float width, float height, float r, float g, float b, float a, float gauge, float level);
	void DrawBorderXY(float x, float y, float z, float width, float height, float r, float g, float b, float a, float level);
	void DrawTexturedRectXY(float x, float y, float z, float sizeX, float sizeY, float r, float g, float b, float a, GLuint texID, float level);
	void DrawTexturedRectSeqXY(float x, float y, float z, float sizeX, float sizeY, float r, float g, float b, float a, GLuint texID, int currSeqX, int currSeqY, int totalSeqX, int totalSeqY, float level);
	void DrawTexturedRect(float x, float y, float z, float size, float r, float g, float b, float a, GLuint texID, float level);
	void DrawTexturedRectSeq(float x, float y, float z, float size, float r, float g, float b, float a, GLuint texID, int currSeqX, int currSeqY, int totalSeqX, int totalSeqY, float level);
	void DrawParticle(float x, float y, float z, float size, float r, float g, float b, float a, float gDirX, float gDirY, GLuint texID, float timeInSeconds, float level);
	void DrawParticleClimate(float x, float y, float z, float size, float r, float g, float b, float a, float gDirX, float gDirY, GLuint texID, float timeInSeconds, float level);
	void DrawText(float x, float y, void* font, float r, float g, float b, char* text);
	void SetSceneTransform(float x, float y, float scaleX, float scaleY);

	unsigned char * loadBMPRaw(const char * imagepath, unsigned int& outWidth, unsigned int& outHeight, bool flipY);

	GLuint CreatePngTexture(char * filePath);
	GLuint CreateBmpTexture(char * filePath);
	void DeleteTexture(GLuint textureID);

private:
	void Initialize(int windowSizeX, int windowSizeY);
	bool ReadFile(char* filename, std::string *target);
	void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType);
	GLuint CompileShaders(char* filenameVS, char* filenameFS);
	void CreateVertexBufferObjects();
	void CreateParticleVBO();
	void CreateParticleCloudVBO();
	void GetGLPosition(float x, float y, float *newX, float *newY);
	void ReleaseAllResources();

	bool m_Initialized = false;
	
	unsigned int m_WindowSizeX = 0;
	unsigned int m_WindowSizeY = 0;

	GLuint m_VBORect = 0;
	GLuint m_VBORectTex = 0;
	GLuint m_VBORectBorder = 0;
	GLuint m_VBOParticles = 0;
	GLuint m_VBOParticleCloud = 0;

	GLuint m_SolidRectShader = 0; 
	GLuint m_SolidRectGaugeShader = 0;
	GLuint m_SolidRectXYShader = 0;
	GLuint m_SolidRectWithTextureShader = 0;
	GLuint m_SolidRectWithTextureSeqShader = 0;
	GLuint m_ParticleWithTextureShader = 0;

	float m_sceneTransX = 0;
	float m_sceneTransY = 0;
	float m_sceneScaleX = 1;
	float m_sceneScaleY = 1;

	GLuint m_textureList[MAX_TEXTURES];

	int m_ParticleCount;
	int m_ParticleVertexCount;
	int m_ParticleCloudCount;
	int m_ParticleCloudVertexCount;
};

