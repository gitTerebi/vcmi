/*
 * ScreenHandler.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

#include "StdInc.h"
#include "ScreenHandler.h"

#include "SDL_Extensions.h"

#include "../CMT.h"
#include "../eventsSDL/NotificationHandler.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../CServerHandler.h"
#include "../GameChatHandler.h"
#include "../gui/CursorHandler.h"
#include "../gui/WindowHandler.h"
#include "../render/Canvas.h"
#include "SDLImage.h"

#include "../../lib/CConfigHandler.h"
#include "../../lib/constants/StringConstants.h"
#include "../../lib/VCMIDirs.h"
#include "../../lib/texts/MetaString.h"

#include <vstd/DateUtils.h>

#ifdef VCMI_ANDROID
#include "../../lib/CAndroidVMHelper.h"
#endif

#ifdef VCMI_IOS
#	include "ios/utils.h"
#endif

#include <SDL.h>
#include <SDL_opengl.h>

// TODO: should be made into a private members of ScreenHandler
SDL_Renderer * mainRenderer = nullptr;

static constexpr Point heroes3Resolution = Point(800, 600);

class OpenGLGpuUpscaler
{
	using GLchar = char;
	using GLsizeiptr = ptrdiff_t;
	using GlCreateShader = GLuint (APIENTRYP)(GLenum type);
	using GlShaderSource = void (APIENTRYP)(GLuint shader, GLsizei count, const GLchar ** string, const GLint * length);
	using GlCompileShader = void (APIENTRYP)(GLuint shader);
	using GlGetShaderiv = void (APIENTRYP)(GLuint shader, GLenum pname, GLint * params);
	using GlGetShaderInfoLog = void (APIENTRYP)(GLuint shader, GLsizei bufSize, GLsizei * length, GLchar * infoLog);
	using GlDeleteShader = void (APIENTRYP)(GLuint shader);
	using GlCreateProgram = GLuint (APIENTRYP)();
	using GlAttachShader = void (APIENTRYP)(GLuint program, GLuint shader);
	using GlLinkProgram = void (APIENTRYP)(GLuint program);
	using GlGetProgramiv = void (APIENTRYP)(GLuint program, GLenum pname, GLint * params);
	using GlGetProgramInfoLog = void (APIENTRYP)(GLuint program, GLsizei bufSize, GLsizei * length, GLchar * infoLog);
	using GlDeleteProgram = void (APIENTRYP)(GLuint program);
	using GlUseProgram = void (APIENTRYP)(GLuint program);
	using GlGetUniformLocation = GLint (APIENTRYP)(GLuint program, const GLchar * name);
	using GlUniform1i = void (APIENTRYP)(GLint location, GLint v0);
	using GlUniform2f = void (APIENTRYP)(GLint location, GLfloat v0, GLfloat v1);
	using GlEnable = void (APIENTRYP)(GLenum cap);
	using GlDisable = void (APIENTRYP)(GLenum cap);
	using GlIsEnabled = GLboolean (APIENTRYP)(GLenum cap);
	using GlGetIntegerv = void (APIENTRYP)(GLenum pname, GLint * data);
	using GlViewport = void (APIENTRYP)(GLint x, GLint y, GLsizei width, GLsizei height);
	using GlMatrixMode = void (APIENTRYP)(GLenum mode);
	using GlPushMatrix = void (APIENTRYP)();
	using GlPopMatrix = void (APIENTRYP)();
	using GlLoadIdentity = void (APIENTRYP)();
	using GlOrtho = void (APIENTRYP)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble nearVal, GLdouble farVal);
	using GlBegin = void (APIENTRYP)(GLenum mode);
	using GlEnd = void (APIENTRYP)();
	using GlTexCoord2f = void (APIENTRYP)(GLfloat s, GLfloat t);
	using GlVertex2f = void (APIENTRYP)(GLfloat x, GLfloat y);
	using GlActiveTexture = void (APIENTRYP)(GLenum texture);
	using GlGenTextures = void (APIENTRYP)(GLsizei n, GLuint * textures);
	using GlDeleteTextures = void (APIENTRYP)(GLsizei n, const GLuint * textures);
	using GlBindTexture = void (APIENTRYP)(GLenum target, GLuint texture);
	using GlTexImage2D = void (APIENTRYP)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void * pixels);
	using GlTexParameteri = void (APIENTRYP)(GLenum target, GLenum pname, GLint param);
	using GlGenFramebuffers = void (APIENTRYP)(GLsizei n, GLuint * ids);
	using GlDeleteFramebuffers = void (APIENTRYP)(GLsizei n, const GLuint * framebuffers);
	using GlBindFramebuffer = void (APIENTRYP)(GLenum target, GLuint framebuffer);
	using GlFramebufferTexture2D = void (APIENTRYP)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
	using GlCheckFramebufferStatus = GLenum (APIENTRYP)(GLenum target);

	GlCreateShader glCreateShader = nullptr;
	GlShaderSource glShaderSource = nullptr;
	GlCompileShader glCompileShader = nullptr;
	GlGetShaderiv glGetShaderiv = nullptr;
	GlGetShaderInfoLog glGetShaderInfoLog = nullptr;
	GlDeleteShader glDeleteShader = nullptr;
	GlCreateProgram glCreateProgram = nullptr;
	GlAttachShader glAttachShader = nullptr;
	GlLinkProgram glLinkProgram = nullptr;
	GlGetProgramiv glGetProgramiv = nullptr;
	GlGetProgramInfoLog glGetProgramInfoLog = nullptr;
	GlDeleteProgram glDeleteProgram = nullptr;
	GlUseProgram glUseProgram = nullptr;
	GlGetUniformLocation glGetUniformLocation = nullptr;
	GlUniform1i glUniform1i = nullptr;
	GlUniform2f glUniform2f = nullptr;
	GlEnable glEnable = nullptr;
	GlDisable glDisable = nullptr;
	GlIsEnabled glIsEnabled = nullptr;
	GlGetIntegerv glGetIntegerv = nullptr;
	GlViewport glViewport = nullptr;
	GlMatrixMode glMatrixMode = nullptr;
	GlPushMatrix glPushMatrix = nullptr;
	GlPopMatrix glPopMatrix = nullptr;
	GlLoadIdentity glLoadIdentity = nullptr;
	GlOrtho glOrtho = nullptr;
	GlBegin glBegin = nullptr;
	GlEnd glEnd = nullptr;
	GlTexCoord2f glTexCoord2f = nullptr;
	GlVertex2f glVertex2f = nullptr;
	GlActiveTexture glActiveTexture = nullptr;
	GlGenTextures glGenTextures = nullptr;
	GlDeleteTextures glDeleteTextures = nullptr;
	GlBindTexture glBindTexture = nullptr;
	GlTexImage2D glTexImage2D = nullptr;
	GlTexParameteri glTexParameteri = nullptr;
	GlGenFramebuffers glGenFramebuffers = nullptr;
	GlDeleteFramebuffers glDeleteFramebuffers = nullptr;
	GlBindFramebuffer glBindFramebuffer = nullptr;
	GlFramebufferTexture2D glFramebufferTexture2D = nullptr;
	GlCheckFramebufferStatus glCheckFramebufferStatus = nullptr;

	GLuint program = 0;
	GLint textureUniform = -1;
	GLint texelUniform = -1;
	GLint sourceSizeUniform = -1;
	GLint passModeUniform = -1;
	GLuint midFramebuffer = 0;
	GLuint midTexture = 0;
	Point midTextureSize;
	bool fsrFilter = false;

	template<typename Function>
	static Function loadFunction(const char * name)
	{
		return reinterpret_cast<Function>(SDL_GL_GetProcAddress(name));
	}

	bool loadFunctions()
	{
		glCreateShader = loadFunction<GlCreateShader>("glCreateShader");
		glShaderSource = loadFunction<GlShaderSource>("glShaderSource");
		glCompileShader = loadFunction<GlCompileShader>("glCompileShader");
		glGetShaderiv = loadFunction<GlGetShaderiv>("glGetShaderiv");
		glGetShaderInfoLog = loadFunction<GlGetShaderInfoLog>("glGetShaderInfoLog");
		glDeleteShader = loadFunction<GlDeleteShader>("glDeleteShader");
		glCreateProgram = loadFunction<GlCreateProgram>("glCreateProgram");
		glAttachShader = loadFunction<GlAttachShader>("glAttachShader");
		glLinkProgram = loadFunction<GlLinkProgram>("glLinkProgram");
		glGetProgramiv = loadFunction<GlGetProgramiv>("glGetProgramiv");
		glGetProgramInfoLog = loadFunction<GlGetProgramInfoLog>("glGetProgramInfoLog");
		glDeleteProgram = loadFunction<GlDeleteProgram>("glDeleteProgram");
		glUseProgram = loadFunction<GlUseProgram>("glUseProgram");
		glGetUniformLocation = loadFunction<GlGetUniformLocation>("glGetUniformLocation");
		glUniform1i = loadFunction<GlUniform1i>("glUniform1i");
		glUniform2f = loadFunction<GlUniform2f>("glUniform2f");
		glEnable = loadFunction<GlEnable>("glEnable");
		glDisable = loadFunction<GlDisable>("glDisable");
		glIsEnabled = loadFunction<GlIsEnabled>("glIsEnabled");
		glGetIntegerv = loadFunction<GlGetIntegerv>("glGetIntegerv");
		glViewport = loadFunction<GlViewport>("glViewport");
		glMatrixMode = loadFunction<GlMatrixMode>("glMatrixMode");
		glPushMatrix = loadFunction<GlPushMatrix>("glPushMatrix");
		glPopMatrix = loadFunction<GlPopMatrix>("glPopMatrix");
		glLoadIdentity = loadFunction<GlLoadIdentity>("glLoadIdentity");
		glOrtho = loadFunction<GlOrtho>("glOrtho");
		glBegin = loadFunction<GlBegin>("glBegin");
		glEnd = loadFunction<GlEnd>("glEnd");
		glTexCoord2f = loadFunction<GlTexCoord2f>("glTexCoord2f");
		glVertex2f = loadFunction<GlVertex2f>("glVertex2f");
		glActiveTexture = loadFunction<GlActiveTexture>("glActiveTexture");
		glGenTextures = loadFunction<GlGenTextures>("glGenTextures");
		glDeleteTextures = loadFunction<GlDeleteTextures>("glDeleteTextures");
		glBindTexture = loadFunction<GlBindTexture>("glBindTexture");
		glTexImage2D = loadFunction<GlTexImage2D>("glTexImage2D");
		glTexParameteri = loadFunction<GlTexParameteri>("glTexParameteri");
		glGenFramebuffers = loadFunction<GlGenFramebuffers>("glGenFramebuffers");
		glDeleteFramebuffers = loadFunction<GlDeleteFramebuffers>("glDeleteFramebuffers");
		glBindFramebuffer = loadFunction<GlBindFramebuffer>("glBindFramebuffer");
		glFramebufferTexture2D = loadFunction<GlFramebufferTexture2D>("glFramebufferTexture2D");
		glCheckFramebufferStatus = loadFunction<GlCheckFramebufferStatus>("glCheckFramebufferStatus");

		return glCreateShader && glShaderSource && glCompileShader && glGetShaderiv && glGetShaderInfoLog && glDeleteShader
			&& glCreateProgram && glAttachShader && glLinkProgram && glGetProgramiv && glGetProgramInfoLog && glDeleteProgram
			&& glUseProgram && glGetUniformLocation && glUniform1i && glUniform2f
			&& glEnable && glDisable && glIsEnabled && glGetIntegerv && glViewport && glMatrixMode && glPushMatrix
			&& glPopMatrix && glLoadIdentity && glOrtho && glBegin && glEnd && glTexCoord2f && glVertex2f
			&& glActiveTexture && glGenTextures && glDeleteTextures && glBindTexture && glTexImage2D && glTexParameteri
			&& glGenFramebuffers && glDeleteFramebuffers && glBindFramebuffer && glFramebufferTexture2D && glCheckFramebufferStatus;
	}

	std::string shaderLog(GLuint shader) const
	{
		std::array<GLchar, 1024> buffer{};
		GLsizei length = 0;
		glGetShaderInfoLog(shader, buffer.size(), &length, buffer.data());
		return std::string(buffer.data(), length);
	}

	std::string programLog(GLuint programToCheck) const
	{
		std::array<GLchar, 1024> buffer{};
		GLsizei length = 0;
		glGetProgramInfoLog(programToCheck, buffer.size(), &length, buffer.data());
		return std::string(buffer.data(), length);
	}

	GLuint compileShader(GLenum type, const char * source)
	{
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, nullptr);
		glCompileShader(shader);

		GLint status = GL_FALSE;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
		if(status != GL_TRUE)
		{
			logGlobal->warn("Failed to compile GPU upscaling shader: %s", shaderLog(shader));
			glDeleteShader(shader);
			return 0;
		}

		return shader;
	}

	bool ensureMidTexture(const Point & size)
	{
		if(midTexture && midTextureSize == size)
			return true;

		if(midTexture)
			glDeleteTextures(1, &midTexture);
		if(midFramebuffer)
			glDeleteFramebuffers(1, &midFramebuffer);
		midTexture = 0;
		midFramebuffer = 0;
		midTextureSize = size;

		glGenTextures(1, &midTexture);
		glBindTexture(GL_TEXTURE_2D, midTexture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		glGenFramebuffers(1, &midFramebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, midFramebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, midTexture, 0);
		bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		if(!complete)
		{
			logGlobal->warn("OpenGL GPU upscaling disabled: intermediate framebuffer is incomplete");
			return false;
		}

		return true;
	}

public:
	OpenGLGpuUpscaler(const std::string & filter)
	{
		if(!loadFunctions())
		{
			logGlobal->warn("OpenGL GPU upscaling disabled: required shader functions are unavailable");
			return;
		}

		static constexpr const char * vertexShaderSource = R"(
			#version 120
			void main()
			{
				gl_Position = ftransform();
				gl_TexCoord[0] = gl_MultiTexCoord0;
			}
		)";

		static constexpr const char * xbrzFragmentShaderSource = R"(
			#version 120
			#define BLEND_NONE 0
			#define BLEND_NORMAL 1
			#define BLEND_DOMINANT 2
			#define LUMW 1.0
			#define EQTOL (30.0 / 255.0)
			#define STEEPT 2.2
			#define DOMT 3.6
			#define MPI 3.1415926535

			uniform sampler2D screenTexture;
			uniform vec2 texelSize;
			uniform vec2 sourceSize;

			float reduce(vec3 c)
			{
				return dot(c, vec3(65536.0, 256.0, 1.0));
			}

			float colorDistance(vec3 a, vec3 b)
			{
				const vec3 w = vec3(0.2627, 0.6780, 0.0593);
				float scaleB = 0.5 / (1.0 - w.b);
				float scaleR = 0.5 / (1.0 - w.r);
				vec3 diff = a - b;
				float y = dot(diff, w);
				float cb = scaleB * (diff.b - y);
				float cr = scaleR * (diff.r - y);
				return sqrt((LUMW * y) * (LUMW * y) + cb * cb + cr * cr);
			}

			bool colorEqual(vec3 a, vec3 b)
			{
				return colorDistance(a, b) < EQTOL;
			}

			vec3 mixIf(vec3 dst, vec3 src, float weight, bool enabled)
			{
				return enabled ? mix(dst, src, weight) : dst;
			}

			void scalePixel(ivec4 blend, vec3 k[9], inout vec3 dst[4])
			{
				float v0 = reduce(k[0]);
				float v4 = reduce(k[4]);
				float v5 = reduce(k[5]);
				float v7 = reduce(k[7]);
				float v8 = reduce(k[8]);
				float d14 = colorDistance(k[1], k[4]);
				float d38 = colorDistance(k[3], k[8]);
				bool shallow = (STEEPT * d14 <= d38) && (v0 != v4) && (v5 != v4);
				bool steep = (STEEPT * d38 <= d14) && (v0 != v8) && (v7 != v8);
				bool need = blend[2] != BLEND_NONE;
				bool doLine = blend[2] >= BLEND_DOMINANT || !(
					(blend[1] != BLEND_NONE && !colorEqual(k[0], k[4])) ||
					(blend[3] != BLEND_NONE && !colorEqual(k[0], k[8])) ||
					(colorEqual(k[4], k[3]) && colorEqual(k[3], k[2]) && colorEqual(k[2], k[1]) && colorEqual(k[1], k[8]) && !colorEqual(k[0], k[2])));
				vec3 blendPixel = colorDistance(k[0], k[1]) <= colorDistance(k[0], k[3]) ? k[1] : k[3];
				dst[1] = mixIf(dst[1], blendPixel, 0.25, need && doLine && steep);
				dst[2] = mixIf(dst[2], blendPixel, doLine ? (shallow ? (steep ? 5.0 / 6.0 : 0.75) : (steep ? 0.75 : 0.5)) : 1.0 - (MPI / 4.0), need);
				dst[3] = mixIf(dst[3], blendPixel, 0.25, need && doLine && shallow);
			}

			void rotateDst(inout vec3 dst[4])
			{
				vec3 tmp = dst[3];
				dst[3] = dst[2];
				dst[2] = dst[1];
				dst[1] = dst[0];
				dst[0] = tmp;
			}

			void main()
			{
				vec2 base = (floor(gl_TexCoord[0].xy * sourceSize) + vec2(0.5)) * texelSize;
				#define SAMPLE(x, y) texture2D(screenTexture, base + vec2(x, y) * texelSize).rgb
				vec3 s[25];
				s[21] = SAMPLE(-1.0, -2.0); s[22] = SAMPLE(0.0, -2.0); s[23] = SAMPLE(1.0, -2.0);
				s[6] = SAMPLE(-1.0, -1.0); s[7] = SAMPLE(0.0, -1.0); s[8] = SAMPLE(1.0, -1.0);
				s[5] = SAMPLE(-1.0, 0.0); s[0] = SAMPLE(0.0, 0.0); s[1] = SAMPLE(1.0, 0.0);
				s[4] = SAMPLE(-1.0, 1.0); s[3] = SAMPLE(0.0, 1.0); s[2] = SAMPLE(1.0, 1.0);
				s[15] = SAMPLE(-1.0, 2.0); s[14] = SAMPLE(0.0, 2.0); s[13] = SAMPLE(1.0, 2.0);
				s[19] = SAMPLE(-2.0, -1.0); s[18] = SAMPLE(-2.0, 0.0); s[17] = SAMPLE(-2.0, 1.0);
				s[9] = SAMPLE(2.0, -1.0); s[10] = SAMPLE(2.0, 0.0); s[11] = SAMPLE(2.0, 1.0);

				float v[9];
				for(int n = 0; n < 9; ++n)
					v[n] = reduce(s[n]);

				ivec4 blend = ivec4(0, 0, 0, 0);
				if(!((v[0] == v[1] && v[3] == v[2]) || (v[0] == v[3] && v[1] == v[2])))
				{
					float d1 = colorDistance(s[4], s[0]) + colorDistance(s[0], s[8]) + colorDistance(s[14], s[2]) + colorDistance(s[2], s[10]) + 4.0 * colorDistance(s[3], s[1]);
					float d2 = colorDistance(s[5], s[3]) + colorDistance(s[3], s[13]) + colorDistance(s[7], s[1]) + colorDistance(s[1], s[11]) + 4.0 * colorDistance(s[0], s[2]);
					bool dominant = (DOMT * d1) < d2;
					blend[2] = ((d1 < d2) && (v[0] != v[1]) && (v[0] != v[3])) ? (dominant ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;
				}
				if(!((v[5] == v[0] && v[4] == v[3]) || (v[5] == v[4] && v[0] == v[3])))
				{
					float d1 = colorDistance(s[17], s[5]) + colorDistance(s[5], s[7]) + colorDistance(s[15], s[3]) + colorDistance(s[3], s[1]) + 4.0 * colorDistance(s[4], s[0]);
					float d2 = colorDistance(s[18], s[4]) + colorDistance(s[4], s[14]) + colorDistance(s[6], s[0]) + colorDistance(s[0], s[2]) + 4.0 * colorDistance(s[5], s[3]);
					bool dominant = (DOMT * d2) < d1;
					blend[3] = ((d2 < d1) && (v[0] != v[5]) && (v[0] != v[3])) ? (dominant ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;
				}
				if(!((v[7] == v[8] && v[0] == v[1]) || (v[7] == v[0] && v[8] == v[1])))
				{
					float d1 = colorDistance(s[5], s[7]) + colorDistance(s[7], s[23]) + colorDistance(s[3], s[1]) + colorDistance(s[1], s[9]) + 4.0 * colorDistance(s[0], s[8]);
					float d2 = colorDistance(s[6], s[0]) + colorDistance(s[0], s[2]) + colorDistance(s[22], s[8]) + colorDistance(s[8], s[10]) + 4.0 * colorDistance(s[7], s[1]);
					bool dominant = (DOMT * d2) < d1;
					blend[1] = ((d2 < d1) && (v[0] != v[7]) && (v[0] != v[1])) ? (dominant ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;
				}
				if(!((v[6] == v[7] && v[5] == v[0]) || (v[6] == v[5] && v[7] == v[0])))
				{
					float d1 = colorDistance(s[18], s[6]) + colorDistance(s[6], s[22]) + colorDistance(s[4], s[0]) + colorDistance(s[0], s[8]) + 4.0 * colorDistance(s[5], s[7]);
					float d2 = colorDistance(s[19], s[5]) + colorDistance(s[5], s[3]) + colorDistance(s[21], s[7]) + colorDistance(s[7], s[1]) + 4.0 * colorDistance(s[6], s[0]);
					bool dominant = (DOMT * d1) < d2;
					blend[0] = ((d1 < d2) && (v[0] != v[5]) && (v[0] != v[7])) ? (dominant ? BLEND_DOMINANT : BLEND_NORMAL) : BLEND_NONE;
				}

				vec3 dst[4];
				dst[0] = s[0]; dst[1] = s[0]; dst[2] = s[0]; dst[3] = s[0];
				if(blend[0] != BLEND_NONE || blend[1] != BLEND_NONE || blend[2] != BLEND_NONE || blend[3] != BLEND_NONE)
				{
					vec3 k[9];
					k[0] = s[0]; k[1] = s[1]; k[2] = s[2]; k[3] = s[3]; k[4] = s[4]; k[5] = s[5]; k[6] = s[6]; k[7] = s[7]; k[8] = s[8];
					scalePixel(blend.xyzw, k, dst);
					k[1] = s[7]; k[2] = s[8]; k[3] = s[1]; k[4] = s[2]; k[5] = s[3]; k[6] = s[4]; k[7] = s[5]; k[8] = s[6];
					rotateDst(dst); scalePixel(blend.wxyz, k, dst);
					k[1] = s[5]; k[2] = s[6]; k[3] = s[7]; k[4] = s[8]; k[5] = s[1]; k[6] = s[2]; k[7] = s[3]; k[8] = s[4];
					rotateDst(dst); scalePixel(blend.zwxy, k, dst);
					k[1] = s[3]; k[2] = s[4]; k[3] = s[5]; k[4] = s[6]; k[5] = s[7]; k[6] = s[8]; k[7] = s[1]; k[8] = s[2];
					rotateDst(dst); scalePixel(blend.yzwx, k, dst);
					rotateDst(dst);
				}

				vec2 fp = fract(gl_TexCoord[0].xy * sourceSize);
				vec3 result = fp.y < 0.5 ? (fp.x < 0.5 ? dst[0] : dst[1]) : (fp.x < 0.5 ? dst[3] : dst[2]);
				gl_FragColor = vec4(result, 1.0);
			}
		)";

		static constexpr const char * xsalFragmentShaderSource = R"(
			#version 120

			uniform sampler2D screenTexture;
			uniform vec2 texelSize;
			uniform vec2 sourceSize;

			void main()
			{
				vec2 cTex = gl_TexCoord[0].xy * sourceSize * 1.00001;
				#define SAMPLE(x, y) texture2D(screenTexture, (floor(cTex + vec2(x, y)) + vec2(0.5)) * texelSize)
				vec4 c00 = SAMPLE(-0.25, -0.25);
				vec4 c20 = SAMPLE( 0.25, -0.25);
				vec4 c02 = SAMPLE(-0.25,  0.25);
				vec4 c22 = SAMPLE( 0.25,  0.25);
				vec4 dt = vec4(1.0);
				float m1 = dot(abs(c00 - c22), dt) + 0.001;
				float m2 = dot(abs(c02 - c20), dt) + 0.001;
				gl_FragColor = (m1 * (c02 + c20) + m2 * (c22 + c00)) / (2.0 * (m1 + m2));
			}
		)";

		static constexpr const char * fsrFragmentShaderSource = R"(
			#version 120

			uniform sampler2D screenTexture;
			uniform vec2 texelSize;
			uniform vec2 sourceSize;
			uniform int passMode;

			vec3 sampleSource(vec2 uv)
			{
				return texture2D(screenTexture, uv).rgb;
			}

			vec3 sharpen(vec2 uv, float amount)
			{
				vec3 center = sampleSource(uv);
				vec3 left = sampleSource(uv + vec2(-texelSize.x, 0.0));
				vec3 right = sampleSource(uv + vec2(texelSize.x, 0.0));
				vec3 top = sampleSource(uv + vec2(0.0, -texelSize.y));
				vec3 bottom = sampleSource(uv + vec2(0.0, texelSize.y));
				vec3 softMin = min(center, min(min(left, right), min(top, bottom)));
				vec3 softMax = max(center, max(max(left, right), max(top, bottom)));
				vec3 edge = center * (1.0 + 4.0 * amount) - amount * (left + right + top + bottom);
				return clamp(edge, softMin, softMax);
			}

			vec3 casScale(vec2 uv)
			{
				vec2 pixel = uv * sourceSize - vec2(0.5);
				vec2 basePixel = floor(pixel) + vec2(0.5);
				vec2 phase = fract(pixel);
				vec2 baseUv = basePixel * texelSize;

				vec3 c00 = sampleSource(baseUv);
				vec3 c10 = sampleSource(baseUv + vec2(texelSize.x, 0.0));
				vec3 c01 = sampleSource(baseUv + vec2(0.0, texelSize.y));
				vec3 c11 = sampleSource(baseUv + texelSize);
				vec3 scaled = mix(mix(c00, c10, phase.x), mix(c01, c11, phase.x), phase.y);

				vec3 left = sampleSource(baseUv + vec2(-texelSize.x, 0.0));
				vec3 right = sampleSource(baseUv + vec2(texelSize.x, 0.0));
				vec3 top = sampleSource(baseUv + vec2(0.0, -texelSize.y));
				vec3 bottom = sampleSource(baseUv + vec2(0.0, texelSize.y));
				vec3 localMin = min(scaled, min(min(left, right), min(top, bottom)));
				vec3 localMax = max(scaled, max(max(left, right), max(top, bottom)));
				vec3 contrast = localMax - localMin;
				float adaptive = clamp(max(contrast.r, max(contrast.g, contrast.b)) * 2.0, 0.0, 1.0);
				vec3 sharpened = scaled * 1.5 - 0.125 * (left + right + top + bottom);
				return clamp(mix(scaled, sharpened, adaptive), localMin, localMax);
			}

			void main()
			{
				vec2 uv = gl_TexCoord[0].xy;
				if(passMode == 1)
					gl_FragColor = vec4(sharpen(uv, 0.16), 1.0);
				else
					gl_FragColor = vec4(casScale(uv), 1.0);
			}
		)";

		fsrFilter = filter == "fsr" || filter == "fsrSharpen";

		const char * fragmentShaderSource =
			fsrFilter ? fsrFragmentShaderSource :
			filter.rfind("xsal", 0) == 0 ? xsalFragmentShaderSource :
			xbrzFragmentShaderSource;

		GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
		GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
		if(!vertexShader || !fragmentShader)
			return;

		program = glCreateProgram();
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		glLinkProgram(program);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		GLint status = GL_FALSE;
		glGetProgramiv(program, GL_LINK_STATUS, &status);
		if(status != GL_TRUE)
		{
			logGlobal->warn("Failed to link GPU upscaling shader: %s", programLog(program));
			glDeleteProgram(program);
			program = 0;
			return;
		}

		textureUniform = glGetUniformLocation(program, "screenTexture");
		texelUniform = glGetUniformLocation(program, "texelSize");
		sourceSizeUniform = glGetUniformLocation(program, "sourceSize");
		passModeUniform = glGetUniformLocation(program, "passMode");
		logGlobal->debug("OpenGL GPU upscaling shader initialized");
	}

	~OpenGLGpuUpscaler()
	{
		if(midTexture)
			glDeleteTextures(1, &midTexture);
		if(midFramebuffer)
			glDeleteFramebuffers(1, &midFramebuffer);
		if(program)
			glDeleteProgram(program);
	}

	bool available() const
	{
		return program != 0;
	}

	void renderQuad(float textureWidth, float textureHeight, bool flipY = false)
	{
		float top = flipY ? textureHeight : 0.0f;
		float bottom = flipY ? 0.0f : textureHeight;

		glBegin(GL_QUADS);
		glTexCoord2f(0.0f, top); glVertex2f(0.0f, 0.0f);
		glTexCoord2f(textureWidth, top); glVertex2f(1.0f, 0.0f);
		glTexCoord2f(textureWidth, bottom); glVertex2f(1.0f, 1.0f);
		glTexCoord2f(0.0f, bottom); glVertex2f(0.0f, 1.0f);
		glEnd();
	}

	void setUpscalerUniforms(const Point & sourceSize, int passMode)
	{
		glUniform1i(textureUniform, 0);
		glUniform2f(texelUniform, 1.0f / sourceSize.x, 1.0f / sourceSize.y);
		glUniform2f(sourceSizeUniform, sourceSize.x, sourceSize.y);
		glUniform1i(passModeUniform, passMode);
	}

	bool render(SDL_Texture * texture, const Point & sourceSize, bool secondPass)
	{
		if(!available())
			return false;
		if(!ensureMidTexture(sourceSize * 2))
			return false;

		SDL_RenderFlush(mainRenderer);

		float textureWidth = 0;
		float textureHeight = 0;
		if(SDL_GL_BindTexture(texture, &textureWidth, &textureHeight) != 0)
		{
			logGlobal->warn("OpenGL GPU upscaling disabled for this frame: %s", SDL_GetError());
			return false;
		}

		GLint viewport[4] = {};
		GLboolean blendEnabled = glIsEnabled(GL_BLEND);
		GLboolean texture2dEnabled = glIsEnabled(GL_TEXTURE_2D);
		glGetIntegerv(GL_VIEWPORT, viewport);

		Point outputSize;
		SDL_GetRendererOutputSize(mainRenderer, &outputSize.x, &outputSize.y);

		glUseProgram(program);
		glActiveTexture(GL_TEXTURE0);
		glDisable(GL_BLEND);

		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		glOrtho(0.0, 1.0, 1.0, 0.0, -1.0, 1.0);
		glMatrixMode(GL_MODELVIEW);
		glPushMatrix();
		glLoadIdentity();

		glBindFramebuffer(GL_FRAMEBUFFER, midFramebuffer);
		glViewport(0, 0, midTextureSize.x, midTextureSize.y);
		setUpscalerUniforms(sourceSize, 0);
		renderQuad(textureWidth, textureHeight);
		SDL_GL_UnbindTexture(texture);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, outputSize.x, outputSize.y);
		glBindTexture(GL_TEXTURE_2D, midTexture);
		if(secondPass)
			setUpscalerUniforms(midTextureSize, fsrFilter ? 1 : 0);
		else
		{
			glUseProgram(0);
			glEnable(GL_TEXTURE_2D);
		}
		renderQuad(1.0f, 1.0f, true);

		glMatrixMode(GL_MODELVIEW);
		glPopMatrix();
		glMatrixMode(GL_PROJECTION);
		glPopMatrix();
		glUseProgram(0);
		if(!texture2dEnabled)
			glDisable(GL_TEXTURE_2D);
		if(blendEnabled)
			glEnable(GL_BLEND);
		glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

		return true;
	}
};

std::tuple<int, int> ScreenHandler::getSupportedScalingRange() const
{
	// H3 resolution, any resolution smaller than that is not correctly supported
	static constexpr Point minResolution = heroes3Resolution;
	// arbitrary limit on *downscaling*. Allow some downscaling, if requested by user. Should be generally limited to 100+ for all but few devices
	static constexpr double minimalScaling = 50;

	Point renderResolution = getRenderResolution();
	double reservedAreaWidth = settings["video"]["reservedWidth"].Float();
	Point availableResolution = Point(renderResolution.x * (1 - reservedAreaWidth), renderResolution.y);
	if(renderResolution.x < renderResolution.y) // reserved in portrait mode
		availableResolution = Point(renderResolution.x, renderResolution.y * (1 - reservedAreaWidth));

	double maximalScalingWidth = 100.0 * availableResolution.x / minResolution.x;
	double maximalScalingHeight = 100.0 * availableResolution.y / minResolution.y;
	double maximalScaling = std::min(maximalScalingWidth, maximalScalingHeight);

	return { minimalScaling, maximalScaling };
}

Rect ScreenHandler::convertLogicalPointsToWindow(const Rect & input) const
{
	Rect result;

	// FIXME: use SDL_RenderLogicalToWindow instead? Needs to be tested on ios

	float scaleX, scaleY;
	SDL_Rect viewport;
	SDL_RenderGetScale(mainRenderer, &scaleX, &scaleY);
	SDL_RenderGetViewport(mainRenderer, &viewport);

#ifdef VCMI_IOS
	// TODO ios: looks like SDL bug actually, try fixing there
	const auto nativeScale = iOS_utils::screenScale();
	scaleX /= nativeScale;
	scaleY /= nativeScale;
#endif

	result.x = (viewport.x + input.x) * scaleX;
	result.y = (viewport.y + input.y) * scaleY;
	result.w = input.w * scaleX;
	result.h = input.h * scaleY;

	return result;
}

int ScreenHandler::getInterfaceScalingPercentage() const
{
	auto [minimalScaling, maximalScaling] = getSupportedScalingRange();

	int userScaling = settings["video"]["resolution"]["scaling"].Integer();

	if (userScaling == 0) // autodetection
	{
#ifdef VCMI_MOBILE
		// for mobiles - stay at maximum scaling unless we have large screen
		// might be better to check screen DPI / physical dimensions, but way more complex, and may result in different edge cases, e.g. chromebooks / tv's
		int preferredMinimalScaling = 200;
#else
		// for PC - avoid downscaling if possible
		int preferredMinimalScaling = 100;
#endif
		// prefer a little below maximum - to give space for extended UI
		int preferredMaximalScaling = maximalScaling * 10 / 12;
		userScaling = std::max(std::min(maximalScaling, preferredMinimalScaling), preferredMaximalScaling);
	}

	int scaling = std::clamp(userScaling, minimalScaling, maximalScaling);
	return scaling;
}

Point ScreenHandler::getPreferredLogicalResolution() const
{
	Point renderResolution = getRenderResolution();
	double reservedAreaWidth = settings["video"]["reservedWidth"].Float();

	int scaling = getInterfaceScalingPercentage();
	Point availableResolution = Point(renderResolution.x * (1 - reservedAreaWidth), renderResolution.y);
	if(renderResolution.x < renderResolution.y) // reserved in portrait mode
		availableResolution = Point(renderResolution.x, renderResolution.y * (1 - reservedAreaWidth));
	Point logicalResolution = availableResolution * 100.0 / scaling;
	return logicalResolution;
}

int ScreenHandler::getScalingFactor() const
{
	switch (upscalingFilter)
	{
		case EUpscalingFilter::NONE: return 1;
		case EUpscalingFilter::XBRZ_2: return 2;
		case EUpscalingFilter::XBRZ_3: return 3;
		case EUpscalingFilter::XBRZ_4: return 4;
	}

	throw std::runtime_error("invalid upscaling filter");
}

Point ScreenHandler::getLogicalResolution() const
{
	return Point(screen->w, screen->h) / getScalingFactor();
}

Point ScreenHandler::getRenderResolution() const
{
	assert(mainRenderer != nullptr);

	Point result;
	SDL_GetRendererOutputSize(mainRenderer, &result.x, &result.y);

	return result;
}

Point ScreenHandler::getPreferredWindowResolution() const
{
	if (getPreferredWindowMode() == EWindowMode::FULLSCREEN_BORDERLESS_WINDOWED)
	{
		SDL_Rect bounds;
		if (SDL_GetDisplayBounds(getPreferredDisplayIndex(), &bounds) == 0)
			return Point(bounds.w, bounds.h);
	}

	const JsonNode & video = settings["video"];
	int width = video["resolution"]["width"].Integer();
	int height = video["resolution"]["height"].Integer();

	return Point(width, height);
}

int ScreenHandler::getPreferredDisplayIndex() const
{
#ifdef VCMI_MOBILE
	// Assuming no multiple screens on Android / ios?
	return 0;
#else
	if (mainWindow != nullptr)
	{
		int result = SDL_GetWindowDisplayIndex(mainWindow);
		if (result >= 0)
			return result;
	}

	return settings["video"]["displayIndex"].Integer();
#endif
}

EWindowMode ScreenHandler::getPreferredWindowMode() const
{
#ifdef VCMI_MOBILE
	// On Android / ios game will always render to screen size
	return EWindowMode::FULLSCREEN_BORDERLESS_WINDOWED;
#else
	const JsonNode & video = settings["video"];
	bool fullscreen = video["fullscreen"].Bool();
	bool realFullscreen = settings["video"]["realFullscreen"].Bool();

	if (!fullscreen)
		return EWindowMode::WINDOWED;

	if (realFullscreen)
		return EWindowMode::FULLSCREEN_EXCLUSIVE;
	else
		return EWindowMode::FULLSCREEN_BORDERLESS_WINDOWED;
#endif
}

ScreenHandler::ScreenHandler()
{
#ifdef VCMI_WINDOWS
	// set VCMI as "per-monitor DPI awareness". This completely disables any DPI-scaling by system.
	// Might not be the best solution since VCMI can't automatically adjust to DPI changes (including moving to monitors with different DPI scaling)
	// However this fixed unintuitive bug where player selects specific resolution for windowed mode, but ends up with completely different one due to scaling
	// NOTE: requires SDL 2.24.
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitor");
#endif
	if(settings["video"]["allowPortrait"].Bool())
		SDL_SetHint(SDL_HINT_ORIENTATIONS, "Portrait PortraitUpsideDown LandscapeLeft LandscapeRight");
	else
		SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

#ifdef VCMI_IOS
	if(!settings["general"]["ignoreMuteSwitch"].Bool())
		SDL_SetHint(SDL_HINT_AUDIO_CATEGORY, "AVAudioSessionCategoryAmbient");
#endif

	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER))
	{
		logGlobal->error("Something was wrong: %s", SDL_GetError());
		exit(-1);
	}

	const auto & logCallback = [](void * userdata, int category, SDL_LogPriority priority, const char * message)
	{
		logGlobal->debug("SDL(category %d; priority %d) %s", category, priority, message);
	};

	SDL_LogSetOutputFunction(logCallback, nullptr);

#ifdef VCMI_ANDROID
	// manually setting egl pixel format, as a possible solution for sdl2<->android problem
	// https://bugzilla.libsdl.org/show_bug.cgi?id=2291
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
#endif // VCMI_ANDROID

	validateSettings();
	recreateWindowAndScreenBuffers();
}

void ScreenHandler::recreateWindowAndScreenBuffers()
{
	destroyScreenBuffers();

	if(mainWindow == nullptr)
		initializeWindow();
	else
		updateWindowState();

	initializeScreenBuffers();

	if(!settings["session"]["headless"].Bool() && settings["general"]["notifications"].Bool())
	{
		NotificationHandler::init(mainWindow);
	}
}

void ScreenHandler::updateWindowState()
{
#ifndef VCMI_MOBILE
	int displayIndex = getPreferredDisplayIndex();

	switch(getPreferredWindowMode())
	{
		case EWindowMode::FULLSCREEN_EXCLUSIVE:
		{
			// for some reason, VCMI fails to switch from FULLSCREEN_BORDERLESS_WINDOWED to FULLSCREEN_EXCLUSIVE directly
			// Switch to windowed mode first to avoid this bug
			SDL_SetWindowFullscreen(mainWindow, 0);
			SDL_SetWindowFullscreen(mainWindow, SDL_WINDOW_FULLSCREEN);

			SDL_DisplayMode mode;
			SDL_GetDesktopDisplayMode(displayIndex, &mode);
			Point resolution = getPreferredWindowResolution();

			mode.w = resolution.x;
			mode.h = resolution.y;

			SDL_SetWindowDisplayMode(mainWindow, &mode);
			SDL_SetWindowPosition(mainWindow, SDL_WINDOWPOS_UNDEFINED_DISPLAY(displayIndex), SDL_WINDOWPOS_UNDEFINED_DISPLAY(displayIndex));

			return;
		}
		case EWindowMode::FULLSCREEN_BORDERLESS_WINDOWED:
		{
			SDL_SetWindowFullscreen(mainWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
			SDL_SetWindowPosition(mainWindow, SDL_WINDOWPOS_UNDEFINED_DISPLAY(displayIndex), SDL_WINDOWPOS_UNDEFINED_DISPLAY(displayIndex));
			return;
		}
		case EWindowMode::WINDOWED:
		{
			Point resolution = getPreferredWindowResolution();
			SDL_SetWindowFullscreen(mainWindow, 0);
			SDL_SetWindowSize(mainWindow, resolution.x, resolution.y);
			if(settings["video"]["windowMaximized"].Bool())
				SDL_MaximizeWindow(mainWindow);
			return;
		}
	}
#endif
}

void ScreenHandler::initializeWindow()
{
	mainWindow = createWindow();

	if(mainWindow == nullptr)
	{
		const char * error = SDL_GetError();
		Point dimensions = getPreferredWindowResolution();

		std::string messagePattern = "Failed to create SDL Window of size %d x %d. Reason: %s";
		std::string message = boost::str(boost::format(messagePattern) % dimensions.x % dimensions.y % error);

		handleFatalError(message, true);
	}

	// create first available renderer if no preferred one is set
	// use no SDL_RENDERER_SOFTWARE or SDL_RENDERER_ACCELERATED flag, so HW accelerated will be preferred but SW renderer will also be possible
	uint32_t rendererFlags = 0;
	if(settings["video"]["vsync"].Bool())
	{
		rendererFlags |= SDL_RENDERER_PRESENTVSYNC;
	}
	mainRenderer = SDL_CreateRenderer(mainWindow, getPreferredRenderingDriver(), rendererFlags);

	if(mainRenderer == nullptr)
	{
		const char * error = SDL_GetError();
		std::string messagePattern = "Failed to create SDL renderer. Reason: %s";
		std::string message = boost::str(boost::format(messagePattern) % error);
		handleFatalError(message, true);
	}

	selectUpscalingFilter();
	selectDownscalingFilter();

	SDL_RendererInfo info;
	SDL_GetRendererInfo(mainRenderer, &info);
	logGlobal->info("Created renderer %s", info.name);

	const auto gpuFilter = settings["video"]["gpuUpscalingFilter"].String();
	if((gpuFilter == "xbrz2" || gpuFilter == "xbrz4" || gpuFilter == "xsal2" || gpuFilter == "xsal4" || gpuFilter == "fsr" || gpuFilter == "fsrSharpen") && std::string(info.name) == "opengl")
		gpuUpscaler = std::make_unique<OpenGLGpuUpscaler>(gpuFilter);
}

EUpscalingFilter ScreenHandler::loadUpscalingFilter() const
{
	static const std::map<std::string, EUpscalingFilter> upscalingFilterTypes =
	{
		{"auto", EUpscalingFilter::AUTO },
		{"none", EUpscalingFilter::NONE },
		{"xbrz2", EUpscalingFilter::XBRZ_2 },
		{"xbrz3", EUpscalingFilter::XBRZ_3 },
		{"xbrz4", EUpscalingFilter::XBRZ_4 }
	};

	auto filterName = settings["video"]["upscalingFilter"].String();
	auto filter = upscalingFilterTypes.count(filterName) ? upscalingFilterTypes.at(filterName) : EUpscalingFilter::AUTO;

	if (filter != EUpscalingFilter::AUTO)
		return filter;

	// else - autoselect
	Point outputResolution = getRenderResolution();
	Point logicalResolution = getPreferredLogicalResolution();

	float scaleX = static_cast<float>(outputResolution.x) / logicalResolution.x;
	float scaleY = static_cast<float>(outputResolution.x) / logicalResolution.x;
	float scaling = std::min(scaleX, scaleY);
	int systemMemoryMb = SDL_GetSystemRAM();

	if (scaling <= 1.001f)
		return EUpscalingFilter::NONE; // running at original resolution or even lower than that - no need for xbrz

	if (systemMemoryMb <= 4096)
		return EUpscalingFilter::NONE; // xbrz2 may use ~1.0 - 1.5 Gb of RAM and has notable CPU cost - avoid on low-spec hardware

	// Only using xbrz2 for autoselection.
	// Higher options may have high system requirements and should be only selected explicitly by player
	return EUpscalingFilter::XBRZ_2;
}

void ScreenHandler::selectUpscalingFilter()
{
	upscalingFilter	= loadUpscalingFilter();
	logGlobal->debug("Selected upscaling filter %d", static_cast<int>(upscalingFilter));
}

void ScreenHandler::selectDownscalingFilter()
{
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, settings["video"]["downscalingFilter"].String().c_str());
	logGlobal->debug("Selected downscaling filter %s", settings["video"]["downscalingFilter"].String());
}

void ScreenHandler::initializeScreenBuffers()
{
#ifdef VCMI_ENDIAN_BIG
	int bmask = 0xff000000;
	int gmask = 0x00ff0000;
	int rmask = 0x0000ff00;
	int amask = 0x000000ff;
#else
	int bmask = 0x000000ff;
	int gmask = 0x0000ff00;
	int rmask = 0x00ff0000;
	int amask = 0xFF000000;
#endif

	auto logicalSize = getPreferredLogicalResolution() * getScalingFactor();
	SDL_RenderSetLogicalSize(mainRenderer, logicalSize.x, logicalSize.y);

	screen = SDL_CreateRGBSurface(0, logicalSize.x, logicalSize.y, 32, rmask, gmask, bmask, amask);
	if(nullptr == screen)
	{
		logGlobal->error("Unable to create surface %dx%d with %d bpp: %s", logicalSize.x, logicalSize.y, 32, SDL_GetError());
		throw std::runtime_error("Unable to create surface");
	}
	//No blending for screen itself. Required for proper cursor rendering.
	SDL_SetSurfaceBlendMode(screen, SDL_BLENDMODE_NONE);

	screenTexture = SDL_CreateTexture(mainRenderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, logicalSize.x, logicalSize.y);

	if(nullptr == screenTexture)
	{
		logGlobal->error("Unable to create screen texture");
		logGlobal->error(SDL_GetError());
		throw std::runtime_error("Unable to create screen texture");
	}

	clearScreen();
}

SDL_Window * ScreenHandler::createWindowImpl(Point dimensions, int flags, bool center)
{
	int displayIndex = getPreferredDisplayIndex();
	int positionFlags = center ? SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex) : SDL_WINDOWPOS_UNDEFINED_DISPLAY(displayIndex);

	return SDL_CreateWindow(GameConstants::VCMI_PROJECT_NAME_VERSIONED, positionFlags, positionFlags, dimensions.x, dimensions.y, flags);
}

SDL_Window * ScreenHandler::createWindow()
{
#ifndef VCMI_MOBILE
	Point dimensions = getPreferredWindowResolution();

	switch(getPreferredWindowMode())
	{
		case EWindowMode::FULLSCREEN_EXCLUSIVE:
			return createWindowImpl(dimensions, SDL_WINDOW_FULLSCREEN, false);

		case EWindowMode::FULLSCREEN_BORDERLESS_WINDOWED:
			return createWindowImpl(Point(), SDL_WINDOW_FULLSCREEN_DESKTOP, false);

		case EWindowMode::WINDOWED:
		{
			int flags = SDL_WINDOW_RESIZABLE;
			if(settings["video"]["windowMaximized"].Bool())
				flags |= SDL_WINDOW_MAXIMIZED;
			return createWindowImpl(dimensions, flags, true);
		}

		default:
			return nullptr;
	};
#endif

#ifdef VCMI_IOS
	SDL_SetHint(SDL_HINT_IOS_HIDE_HOME_INDICATOR, "1");
	SDL_SetHint(SDL_HINT_RETURN_KEY_HIDES_IME, "1");

	uint32_t windowFlags = SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALLOW_HIGHDPI;
	SDL_Window * result = createWindowImpl(Point(), windowFlags | SDL_WINDOW_METAL, false);

	if(result != nullptr)
		return result;

	logGlobal->warn("Metal unavailable, using OpenGLES");
	return createWindowImpl(Point(), windowFlags, false);
#endif

#ifdef VCMI_ANDROID
	return createWindowImpl(Point(), SDL_WINDOW_RESIZABLE, false);
#endif
}

bool ScreenHandler::onScreenResize(bool keepWindowResolution)
{
	if (keepWindowResolution)
	{
		// Only allowed in windowed mode
		if (getPreferredWindowMode() != EWindowMode::WINDOWED)
			return false;

		auto res = getRenderResolution();

		if (res.x < heroes3Resolution.x || res.y < heroes3Resolution.y)
			return false;

		Settings video = settings.write["video"];
		video["resolution"]["width"].Integer() = res.x;
		video["resolution"]["height"].Integer() = res.y;

		// Only recreate buffers (no window changes!)
		destroyScreenBuffers();
		initializeScreenBuffers();
	}
	else
	{
		// Apply settings change (may resize window / change fullscreen)
		recreateWindowAndScreenBuffers();
	}

	return true;
}

void ScreenHandler::validateSettings()
{
#ifndef VCMI_MOBILE
	{
		int displayIndex = settings["video"]["displayIndex"].Integer();
		int displaysCount = SDL_GetNumVideoDisplays();

		if (displayIndex >= displaysCount)
		{
			Settings writer = settings.write["video"]["displayIndex"];
			writer->Float() = 0;
		}
	}

	if (getPreferredWindowMode() == EWindowMode::WINDOWED)
	{
		//we only check that our desired window size fits on screen
		int displayIndex = getPreferredDisplayIndex();
		Point resolution = getPreferredWindowResolution();

		SDL_DisplayMode mode;

		if (SDL_GetDesktopDisplayMode(displayIndex, &mode) == 0)
		{
			if(resolution.x > mode.w || resolution.y > mode.h)
			{
				Settings writer = settings.write["video"]["resolution"];
				writer["width"].Float() = mode.w;
				writer["height"].Float() = mode.h;
			}
		}
	}

	if (getPreferredWindowMode() == EWindowMode::FULLSCREEN_EXCLUSIVE)
	{
		auto legalOptions = getSupportedResolutions();
		Point selectedResolution = getPreferredWindowResolution();

		if(!vstd::contains(legalOptions, selectedResolution))
		{
			// resolution selected for fullscreen mode is not supported by display
			// try to find current display resolution and use it instead as "reasonable default"
			SDL_DisplayMode mode;

			if (SDL_GetDesktopDisplayMode(getPreferredDisplayIndex(), &mode) == 0)
			{
				Settings writer = settings.write["video"]["resolution"];
				writer["width"].Float() = mode.w;
				writer["height"].Float() = mode.h;
			}
		}
	}
#endif
}

int ScreenHandler::getPreferredRenderingDriver() const
{
	int result = -1;
	const JsonNode & video = settings["video"];

	int driversCount = SDL_GetNumRenderDrivers();
	std::string preferredDriverName = video["driver"].String();

	logGlobal->info("Found %d render drivers", driversCount);

	for(int it = 0; it < driversCount; it++)
	{
		SDL_RendererInfo info;
		if (SDL_GetRenderDriverInfo(it, &info) == 0)
		{
			std::string driverName(info.name);

			if(!preferredDriverName.empty() && driverName == preferredDriverName)
			{
				result = it;
				logGlobal->info("\t%s (active)", driverName);
			}
			else
				logGlobal->info("\t%s", driverName);
		}
		else
			logGlobal->info("\t(error)");
	}
	return result;
}

void ScreenHandler::destroyScreenBuffers()
{
	if(nullptr != screen)
	{
		SDL_FreeSurface(screen);
		screen = nullptr;
	}

	if(nullptr != screenTexture)
	{
		SDL_DestroyTexture(screenTexture);
		screenTexture = nullptr;
	}

	gpuUpscaler.reset();
}

void ScreenHandler::destroyWindow()
{
	if(nullptr != mainRenderer)
	{
		SDL_DestroyRenderer(mainRenderer);
		mainRenderer = nullptr;
	}

	if(nullptr != mainWindow)
	{
		SDL_DestroyWindow(mainWindow);
		mainWindow = nullptr;
	}
}

ScreenHandler::~ScreenHandler()
{
	if(settings["general"]["notifications"].Bool())
		NotificationHandler::destroy();

	destroyScreenBuffers();
	destroyWindow();
	SDL_Quit();
}

void ScreenHandler::clearScreen()
{
	SDL_SetRenderDrawColor(mainRenderer, 0, 0, 0, 255);
	SDL_RenderClear(mainRenderer);
	SDL_RenderPresent(mainRenderer);
}

Canvas ScreenHandler::getScreenCanvas() const
{
	return Canvas::createFromSurface(screen, CanvasScalingPolicy::AUTO);
}

void ScreenHandler::updateScreenTexture()
{
	if(colorScheme == ColorScheme::NONE)
	{
		SDL_UpdateTexture(screenTexture, nullptr, screen->pixels, screen->pitch);
		return;
	}

	SDL_Surface * screenScheme = SDL_ConvertSurface(screen, screen->format, screen->flags);
	if(colorScheme == ColorScheme::GRAYSCALE)
		CSDL_Ext::convertToGrayscale(screenScheme, Rect(0, 0, screen->w, screen->h));
	else if(colorScheme == ColorScheme::H2_SCHEME)
		CSDL_Ext::convertToH2Scheme(screenScheme, Rect(0, 0, screen->w, screen->h));
	SDL_UpdateTexture(screenTexture, nullptr, screenScheme->pixels, screenScheme->pitch);
	SDL_FreeSurface(screenScheme);
}

void ScreenHandler::presentScreenTexture()
{
	SDL_RenderClear(mainRenderer);

	bool renderedWithGpuUpscaler = false;
	if(gpuUpscaler && gpuUpscaler->available())
	{
		const auto gpuFilter = settings["video"]["gpuUpscalingFilter"].String();
		renderedWithGpuUpscaler = gpuUpscaler->render(screenTexture, Point(screen->w, screen->h), gpuFilter == "xbrz4" || gpuFilter == "xsal4" || gpuFilter == "fsrSharpen");
	}

	if(!renderedWithGpuUpscaler)
		SDL_RenderCopy(mainRenderer, screenTexture, nullptr, nullptr);

	ENGINE->cursor().render();
	SDL_RenderPresent(mainRenderer);
}

std::vector<Point> ScreenHandler::getSupportedResolutions() const
{
	int displayID = getPreferredDisplayIndex();
	return getSupportedResolutions(displayID);
}

std::vector<Point> ScreenHandler::getSupportedResolutions( int displayIndex) const
{
	//NOTE: this method is never called on Android/iOS, only on desktop systems

	std::vector<Point> result;

	int modesCount = SDL_GetNumDisplayModes(displayIndex);

	for (int i =0; i < modesCount; ++i)
	{
		SDL_DisplayMode mode;
		if (SDL_GetDisplayMode(displayIndex, i, &mode) == 0)
		{
			Point resolution(mode.w, mode.h);
			result.push_back(resolution);
		}
	}

	std::ranges::sort(result, [](const auto & left, const auto & right)
	{
		return left.x * left.y < right.x * right.y;
	});

	// erase potential duplicates, e.g. resolutions with different framerate / bits per pixel
	result.erase(std::ranges::unique(result).end(), result.end());

	return result;
}

bool ScreenHandler::hasFocus()
{
	ui32 flags = SDL_GetWindowFlags(mainWindow);
	return flags & SDL_WINDOW_INPUT_FOCUS;
}

void ScreenHandler::setColorScheme(ColorScheme scheme)
{
	colorScheme = scheme;
}

void ScreenHandler::screenShot() const
{
	const boost::filesystem::path outPath = VCMIDirs::get().userExtractedPath() / "screenshots";
	boost::filesystem::create_directories(outPath);
	const boost::filesystem::path filePath = outPath / ("screenshot-" + vstd::getDateTimeISO8601Basic(std::time(nullptr)) + ".png");
	auto img = std::make_shared<SDLImageShared>(screen);
	img->exportBitmap(filePath, nullptr);
	MetaString txt;
	txt.appendTextID("vcmi.client.screenShot");
	txt.replaceRawString(filePath.string());
	if(GAME->interface())
		GAME->server().getGameChat().sendMessageGameplay(txt.toString());
}
