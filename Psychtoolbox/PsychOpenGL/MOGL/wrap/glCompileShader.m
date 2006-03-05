function glCompileShader( shader )

% glCompileShader  Interface to OpenGL function glCompileShader
%
% usage:  glCompileShader( shader )
%
% C function:  void glCompileShader(GLuint shader)

% 05-Mar-2006 -- created (generated automatically from header files)

if nargin~=1,
    error('invalid number of arguments');
end

moglcore( 'glCompileShader', shader );

return
