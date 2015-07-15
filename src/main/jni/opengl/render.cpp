#include <jni.h>

#include <stdio.h>
#include <string.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "image.h"
#include "loger.h"
#include "render.h"

typedef struct opengl_t {
	GLuint	m_hProgramObject;
	GLuint	m_nTextureIds[2];
	GLuint	m_nBufs[3];
	int	m_bTexInit;
	int	m_bMirror;
	int	m_nDisplayOrientation;
	int	m_nPixelFormat;
}OPENGLES, *LPOPENGLES;

const char* pVertexShaderStr =
      "uniform float u_offset;      \n"
      "attribute vec4 a_position;   \n"
      "attribute vec2 a_texCoord;   \n"
      "varying highp vec2 v_texCoord;     \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = a_position; \n"
      //"   gl_Position.x += u_offset;\n"
      "   v_texCoord = a_texCoord;  \n"
      "}                            \n";

/*
const char* pfGalaxyShader = 
"precision highp float;\n"
"uniform sampler2D y_texture;\n"
"uniform sampler2D uv_texture;\n"
"varying highp vec2 v_texCoord;\n"
"void main()\n"
"{\n"
"    mediump vec3 yuv;\n"
"    highp vec3 rgb; \n"
"    yuv.x = texture2D(y_texture, v_texCoord).r;  \n"
"    yuv.y = texture2D(uv_texture, v_texCoord).a-0.5;\n"
 "   yuv.z = texture2D(uv_texture, v_texCoord).r-0.5;\n"
 "   rgb = mat3(      1,       1,       1,\n"
 "              0, -.21482, 2.12798,\n"
 "              1.28033, -.38059,       0) * yuv;\n"
 "   gl_FragColor = vec4(rgb, 1);\n"
"}\n";
*/

const char* pFragmentShaderNV21 =
"precision highp float;\n"
"uniform sampler2D y_texture;\n"
"uniform sampler2D uv_texture;\n"
"varying highp vec2 v_texCoord;\n"
"void main()\n"
"{\n"
"    mediump vec3 yuv;\n"
"    highp vec3 rgb; \n"
"    yuv.x = texture2D(y_texture, v_texCoord).r;  \n"
"    yuv.y = texture2D(uv_texture, v_texCoord).a-0.5;\n"
 "   yuv.z = texture2D(uv_texture, v_texCoord).r-0.5;\n"
 "   rgb = mat3(      1,       1,       1,\n"
 "              0, -0.344, 1.770,\n"
 "              1.403, -0.714,       0) * yuv;\n"
 "   gl_FragColor = vec4(rgb, 1);\n"
"}\n";

const char* pFragmentShaderNV12 =
"precision highp float;\n"
"uniform sampler2D y_texture;\n"
"uniform sampler2D uv_texture;\n"
"varying highp vec2 v_texCoord;\n"
"void main()\n"
"{\n"
"    mediump vec3 yuv;\n"
"    highp vec3 rgb; \n"
"    yuv.x = texture2D(y_texture, v_texCoord).r;  \n"
"    yuv.y = texture2D(uv_texture, v_texCoord).r-0.5;\n"
 "   yuv.z = texture2D(uv_texture, v_texCoord).a-0.5;\n"
 "   rgb = mat3(      1,       1,       1,\n"
 "              0, -0.344, 1.770,\n"
 "              1.403, -0.714,       0) * yuv;\n"
 "   gl_FragColor = vec4(rgb, 1);\n"
"}\n";


static GLuint LoadShader(GLenum shaderType, const char* pSource);
static void CreateTextures(LPOPENGLES engine);

int GLInit(int mirror, int ori)
{
	LPOPENGLES engine;
	GLuint	vertexShader;
	GLuint	fragmentShader;
	GLint	linked;

	LOGD("glesInit() <---");

	engine = (LPOPENGLES)malloc(sizeof(OPENGLES));
	engine->m_hProgramObject		= 0;
	engine->m_bTexInit				= -1;
	engine->m_bMirror				= mirror;
	engine->m_nDisplayOrientation	= ori;

	vertexShader = LoadShader(GL_VERTEX_SHADER, pVertexShaderStr);
	fragmentShader = LoadShader(GL_FRAGMENT_SHADER, pFragmentShaderNV21);

	engine->m_hProgramObject = glCreateProgram();
	if (0 == engine->m_hProgramObject) {
		LOGE("create programObject failed");
		return 0;
	}

	LOGD("glAttachShader");

	glAttachShader(engine->m_hProgramObject, vertexShader);
	glAttachShader(engine->m_hProgramObject, fragmentShader);

	LOGD("glBindAttribLocation");
	glBindAttribLocation(engine->m_hProgramObject, 0, "a_position");
	glBindAttribLocation(engine->m_hProgramObject, 1, "a_texCoord");

	glLinkProgram ( engine->m_hProgramObject );

	glGetProgramiv( engine->m_hProgramObject, GL_LINK_STATUS, &linked);
	if (0 == linked) {
		GLint	infoLen = 0;
		LOGE("link failed");
		glGetProgramiv( engine->m_hProgramObject, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1) {
			char* infoLog = (char*)malloc(sizeof(char) * infoLen);

			glGetProgramInfoLog( engine->m_hProgramObject, infoLen, NULL, infoLog);
			LOGE( "Error linking program: %s", infoLog);

			free(infoLog);
			infoLog = NULL;
		}

		glDeleteProgram( engine->m_hProgramObject);
		return 0;
	}

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glEnable(GL_TEXTURE_2D);

	CreateTextures(engine);

	glGenBuffers(3, engine->m_nBufs);

	float scale_factor = 1.0f;
	GLfloat vVertices[] = { -scale_factor,  scale_factor, 0.0f, 1.0f,  // Position 0
                           -scale_factor, -scale_factor, 0.0f, 1.0f, // Position 1
                            scale_factor, -scale_factor, 0.0f, 1.0f, // Position 2
                            scale_factor,  scale_factor, 0.0f, 1.0f,  // Position 3
                         };

	GLfloat tCoords[] = {0.0f,  0.0f, 
						 0.0f,  1.0f, 
						 1.0f,  1.0f, 
						 1.0f,  0.0f};
	int degree = 0;
	while (engine->m_nDisplayOrientation > degree) {
		GLfloat temp[2];
		degree += 90;
		temp[0] = tCoords[0]; temp[1] = tCoords[1];
		tCoords[0] = tCoords[2]; tCoords[1] = tCoords[3];
		tCoords[2] = tCoords[4]; tCoords[3] = tCoords[5];
		tCoords[4] = tCoords[6]; tCoords[5] = tCoords[7];
		tCoords[6] = tCoords[0]; tCoords[7] = tCoords[1];
	}
		
	if (engine->m_bMirror){
		GLfloat temp[2];
		LOGD("set mirror is true");
		temp[0] = tCoords[0]; temp[1] = tCoords[2];
		tCoords[0] = tCoords[4]; tCoords[2] = tCoords[6];
		tCoords[4] = temp[0]; tCoords[6] = temp[1];
	}

	GLushort indexs[] = { 0, 1, 2, 0, 2, 3 };

	glBindBuffer(GL_ARRAY_BUFFER, engine->m_nBufs[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*16, vVertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, engine->m_nBufs[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat)*8, tCoords, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, engine->m_nBufs[2]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLushort)*6, indexs, GL_STATIC_DRAW);

	LOGD("glesInit() --->");

	return (int)engine;
}

void GLChanged(int handle, int w, int h)
{
	LPOPENGLES engine = (LPOPENGLES)handle;
	LOGD("glesChanged(%d, %d) <---", w, h);
	engine->m_bTexInit = -1;
	glViewport(0, 0, w, h);
	LOGD("glesChanged() --->");
}

void GLRender(int handle, unsigned char* pData, int w, int h, int format)
{
	LPOPENGLES engine = (LPOPENGLES)handle;
	if (pData == NULL) {
		LOGE("pOffScreen == MNull");
		return;
	}
	
	if (glIsTexture (engine->m_nTextureIds[0])) {
		glDeleteTextures(1, &engine->m_nTextureIds[0]);
	}

	if (glIsTexture (engine->m_nTextureIds[1])) {
		glDeleteTextures(1, &engine->m_nTextureIds[1]);
	}

	CreateTextures(engine);
	engine->m_bTexInit = -1;

	glClear ( GL_COLOR_BUFFER_BIT );

	if (engine->m_bTexInit == 1) {
		glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[0]);
		//NV21 Y
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_LUMINANCE, GL_UNSIGNED_BYTE, pData);

		glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[1]);
		//NV21 UV
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w >> 1, h >> 1, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, pData + w * h);
	} else {
		glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[0]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, w, h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, pData);

		glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[1]);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, w >> 1, h >> 1, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, pData + w * h);
		engine->m_bTexInit = 1;
	}

	glUseProgram ( engine->m_hProgramObject );

	GLuint textureUniformY = glGetUniformLocation(engine->m_hProgramObject, "y_texture");
	GLuint textureUniformU = glGetUniformLocation(engine->m_hProgramObject, "uv_texture");

	glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[0]);
	glUniform1i(textureUniformY, 0);

	glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[1]);
	glUniform1i(textureUniformU, 1);

	glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	glTexParameteri ( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

	glBindBuffer(GL_ARRAY_BUFFER, engine->m_nBufs[0]);
	glVertexAttribPointer ( 0, 4, GL_FLOAT, 
			   GL_FALSE, 4 * sizeof(GLfloat), 0 );
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, engine->m_nBufs[1]);
	glVertexAttribPointer ( 1, 2, GL_FLOAT,
			   GL_FALSE, 2 * sizeof(GLfloat), 0 );
	glEnableVertexAttribArray(1);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, engine->m_nBufs[2]);
	glDrawElements ( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0 );

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);

	glBindTexture(GL_TEXTURE_2D, 0);
}

void GLUnInit(int handle)
{
	LPOPENGLES engine = (LPOPENGLES)handle;
	free(engine);
}


GLuint LoadShader(GLenum shaderType, const char* pSource)
{
    GLuint shader = 0;
	shader = glCreateShader(shaderType);
	LOGD("glGetShaderiv called  shader = %d GL_INVALID_ENUM = %d GL_INVALID_OPERATION = %d", shader, GL_INVALID_ENUM, GL_INVALID_OPERATION);
    if (shader) {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 1;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        LOGD( "glGetShaderiv called compiled = %d, shader = %d", compiled, shader);
        if (!compiled)
		{
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen)
			{
                char* buf = (char*) malloc(infoLen);
                if (buf)
				{
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    LOGE("Could not compile shader %d: %s",
                            shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader); // �ͷ�
                shader = 0;
            }
			return 0;
        }
    }
    return shader;
}

void CreateTextures(LPOPENGLES engine)
{
	glGenTextures(2, engine->m_nTextureIds);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, engine->m_nTextureIds[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

