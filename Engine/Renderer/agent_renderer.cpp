#include "Engine/Renderer/agent_renderer.h"

#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cstddef>
#include <iostream>
#include <string>

namespace engine::renderer {
namespace {

constexpr char kVertexShaderSource[] = R"(
#version 330 core

layout(location = 0) in vec3 local_position;
layout(location = 1) in mat4 model;

uniform mat4 view_projection;

out float local_depth_shade;
out vec3 world_position;

void main() {
  vec4 world = model * vec4(local_position, 1.0);
  gl_Position = view_projection * world;
  local_depth_shade = local_position.z;
  world_position = world.xyz;
}
)";

constexpr char kFragmentShaderSource[] = R"(
#version 330 core

in float local_depth_shade;
in vec3 world_position;
out vec4 fragment_color;

uniform vec3 camera_position;

void main() {
  vec3 dark_color = vec3(0.02, 0.25, 0.45);
  vec3 bright_color = vec3(0.05, 0.65, 0.85);
  float surface_blend = clamp(local_depth_shade + 0.4, 0.0, 1.0);
  vec3 agent_color = mix(dark_color, bright_color, surface_blend);

  float camera_distance = length(world_position - camera_position);
  float fog = smoothstep(60.0, 140.0, camera_distance) * 0.65;
  fragment_color = vec4(mix(agent_color, vec3(1.0), fog), 1.0);
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
  std::cerr << "Failed to compile OpenGL shader:\n" << log << '\n';
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
  std::cerr << "Failed to link OpenGL shader program:\n" << log << '\n';
  glDeleteProgram(program);
  return 0U;
}

constexpr std::array<glm::vec3, 12> kAgentVertices{{
    {0.0F, 0.0F, 0.8F},
    {-0.35F, -0.2F, -0.4F},
    {0.35F, -0.2F, -0.4F},
    {0.0F, 0.0F, 0.8F},
    {0.35F, -0.2F, -0.4F},
    {0.0F, 0.35F, -0.4F},
    {0.0F, 0.0F, 0.8F},
    {0.0F, 0.35F, -0.4F},
    {-0.35F, -0.2F, -0.4F},
    {-0.35F, -0.2F, -0.4F},
    {0.0F, 0.35F, -0.4F},
    {0.35F, -0.2F, -0.4F},
}};

}  // namespace

AgentRenderer::~AgentRenderer() { Shutdown(); }

bool AgentRenderer::Initialize() {
  Shutdown();

  shader_program_ = CreateShaderProgram();
  if (shader_program_ == 0U) {
    return false;
  }

  view_projection_location_ =
      glGetUniformLocation(shader_program_, "view_projection");
  camera_position_location_ =
      glGetUniformLocation(shader_program_, "camera_position");
  if (view_projection_location_ < 0 || camera_position_location_ < 0) {
    std::cerr << "Failed to find agent shader uniforms\n";
    Shutdown();
    return false;
  }

  glGenVertexArrays(1, &vertex_array_);
  glGenBuffers(1, &mesh_buffer_);
  glGenBuffers(1, &instance_buffer_);

  glBindVertexArray(vertex_array_);

  glBindBuffer(GL_ARRAY_BUFFER, mesh_buffer_);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(sizeof(kAgentVertices)),
               kAgentVertices.data(), GL_STATIC_DRAW);
  glEnableVertexAttribArray(0U);
  glVertexAttribPointer(0U, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);

  glBindBuffer(GL_ARRAY_BUFFER, instance_buffer_);
  for (unsigned int column = 0U; column < 4U; ++column) {
    const unsigned int attribute = 1U + column;
    const std::size_t offset = column * sizeof(glm::vec4);
    glEnableVertexAttribArray(attribute);
    glVertexAttribPointer(attribute, 4, GL_FLOAT, GL_FALSE,
                          sizeof(glm::mat4),
                          reinterpret_cast<const void*>(offset));
    glVertexAttribDivisor(attribute, 1U);
  }

  glBindBuffer(GL_ARRAY_BUFFER, 0U);
  glBindVertexArray(0U);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  return true;
}

void AgentRenderer::Render(std::span<const glm::mat4> model_matrices,
                           const glm::mat4& view_projection,
                           const glm::vec3& camera_position) {
  if (shader_program_ == 0U || model_matrices.empty()) {
    return;
  }

  glUseProgram(shader_program_);
  glUniformMatrix4fv(view_projection_location_, 1, GL_FALSE,
                     glm::value_ptr(view_projection));
  glUniform3fv(camera_position_location_, 1, glm::value_ptr(camera_position));

  glBindVertexArray(vertex_array_);
  glBindBuffer(GL_ARRAY_BUFFER, instance_buffer_);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(model_matrices.size_bytes()),
               model_matrices.data(), GL_DYNAMIC_DRAW);

  glDrawArraysInstanced(GL_TRIANGLES, 0,
                        static_cast<int>(kAgentVertices.size()),
                        static_cast<int>(model_matrices.size()));

  glBindBuffer(GL_ARRAY_BUFFER, 0U);
  glBindVertexArray(0U);
  glUseProgram(0U);
}

void AgentRenderer::Shutdown() {
  if (instance_buffer_ != 0U) {
    glDeleteBuffers(1, &instance_buffer_);
    instance_buffer_ = 0U;
  }

  if (mesh_buffer_ != 0U) {
    glDeleteBuffers(1, &mesh_buffer_);
    mesh_buffer_ = 0U;
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
  camera_position_location_ = -1;
}

}  // namespace engine::renderer
