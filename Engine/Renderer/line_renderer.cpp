#include "Engine/Renderer/line_renderer.h"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>

#include <cstddef>
#include <iostream>
#include <string>

namespace engine::renderer {
namespace {

constexpr char kVertexShaderSource[] = R"(
#version 330 core

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec3 vertex_color;

uniform mat4 view_projection;

out vec3 line_color;

void main() {
  gl_Position = view_projection * vec4(vertex_position, 1.0);
  line_color = vertex_color;
}
)";

constexpr char kFragmentShaderSource[] = R"(
#version 330 core

in vec3 line_color;
out vec4 fragment_color;

void main() {
  fragment_color = vec4(line_color, 1.0);
}
)";

unsigned int CompileShader(unsigned int type, const char* source) {
  const unsigned int shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  int succeeded = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &succeeded);
  if (succeeded == GL_TRUE) {
    return shader;
  }

  int log_length = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
  std::string log(static_cast<std::size_t>(log_length), '\0');
  glGetShaderInfoLog(shader, log_length, nullptr, log.data());
  std::cerr << "Failed to compile line shader:\n" << log << '\n';
  glDeleteShader(shader);
  return 0U;
}

unsigned int CreateShaderProgram() {
  const unsigned int vertex_shader =
      CompileShader(GL_VERTEX_SHADER, kVertexShaderSource);
  if (vertex_shader == 0U) {
    return 0U;
  }

  const unsigned int fragment_shader =
      CompileShader(GL_FRAGMENT_SHADER, kFragmentShaderSource);
  if (fragment_shader == 0U) {
    glDeleteShader(vertex_shader);
    return 0U;
  }

  const unsigned int program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  int succeeded = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &succeeded);
  if (succeeded == GL_TRUE) {
    return program;
  }

  int log_length = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
  std::string log(static_cast<std::size_t>(log_length), '\0');
  glGetProgramInfoLog(program, log_length, nullptr, log.data());
  std::cerr << "Failed to link line shader program:\n" << log << '\n';
  glDeleteProgram(program);
  return 0U;
}

}  // namespace

LineRenderer::~LineRenderer() { Shutdown(); }

bool LineRenderer::Initialize(std::span<const LineVertex> vertices) {
  Shutdown();

  shader_program_ = CreateShaderProgram();
  if (shader_program_ == 0U) {
    return false;
  }

  view_projection_location_ =
      glGetUniformLocation(shader_program_, "view_projection");
  if (view_projection_location_ < 0) {
    std::cerr << "Failed to find line shader uniform\n";
    Shutdown();
    return false;
  }

  glGenVertexArrays(1, &vertex_array_);
  glGenBuffers(1, &vertex_buffer_);
  glBindVertexArray(vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size_bytes()),
               vertices.data(), GL_STATIC_DRAW);

  glEnableVertexAttribArray(0U);
  glVertexAttribPointer(0U, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
                        reinterpret_cast<const void*>(
                            offsetof(LineVertex, position)));
  glEnableVertexAttribArray(1U);
  glVertexAttribPointer(
      1U, 3, GL_FLOAT, GL_FALSE, sizeof(LineVertex),
      reinterpret_cast<const void*>(offsetof(LineVertex, color)));

  glBindBuffer(GL_ARRAY_BUFFER, 0U);
  glBindVertexArray(0U);
  vertex_count_ = vertices.size();
  return true;
}

void LineRenderer::Render(const glm::mat4& view_projection) {
  if (shader_program_ == 0U || vertex_count_ == 0U) {
    return;
  }

  glUseProgram(shader_program_);
  glUniformMatrix4fv(view_projection_location_, 1, GL_FALSE,
                     glm::value_ptr(view_projection));
  glBindVertexArray(vertex_array_);
  glDrawArrays(GL_LINES, 0, static_cast<int>(vertex_count_));
  glBindVertexArray(0U);
  glUseProgram(0U);
}

void LineRenderer::Shutdown() {
  if (vertex_buffer_ != 0U) {
    glDeleteBuffers(1, &vertex_buffer_);
    vertex_buffer_ = 0U;
  }

  if (vertex_array_ != 0U) {
    glDeleteVertexArrays(1, &vertex_array_);
    vertex_array_ = 0U;
  }

  if (shader_program_ != 0U) {
    glDeleteProgram(shader_program_);
    shader_program_ = 0U;
  }

  view_projection_location_ = -1;
  vertex_count_ = 0U;
}

}  // namespace engine::renderer
