function handle = LoadGLSLProgramFromFiles(filenames, debug)
% handle = LoadGLSLProgramFromFiles(filenames, [, debug])
% Loads a GLSL OpenGL shading language program. All shader definition files in
% 'filenames' are read, shaders are built for each definition and all
% shaders are linked together into a new GLSL program. Returns a handle to
% the new program, if successfull. The optional 'debug' flag allows to enable
% debugging output.
%
% The program can then be used at any time by calling glUseProgram(handle). One
% can switch back to the standard OpenGL fixed-function pipeline by calling
% glUseProgram(0).
%
% 'filenames' can have one of two formats: If filenames is a array of
% strings that define the names of the shaders to use, then all shader
% files are loaded, compiled and linked into a single program. E.g.,
% shaderfiles={ 'myshader.vert' , 'myothershader.frag'}; will try to load
% the two shaderfiles myshader.vert and myothershader.frag and link them
% into a valid program.
%
% If only a single filename is given, then all shaders beginning with that
% name are linked into a program. E.g., shaderfiles = 'Phonglighting' will
% try to link all files starting with Phonglighting.

% 29-Mar-2006 written by MK

global GL;

if isempty(GL)
    InitializeMatlabOpenGL;
end;

if nargin < 2
    debug = 0;
end;

if nargin < 1 | isempty(filenames)
    error('No filenames for GLSL program provided! Aborted.');
end;

% Make sure we run on a GLSL capable system.
AssertGLSL;

% Create new program object and get handle to it:
handle = glCreateProgram;

if ischar(filenames)
    % One single name given. Load and link all shaders starting with that
    % name:
    if debug>1
        fprintf('Compiling all shaders matching %s * into a GLSL program...\n', filenames);
    end;

    % Add default shader path if no path is specified as part of
    % 'filenames':
    if isempty(fileparts(filenames))
        filenames = [ PsychtoolboxRoot 'PsychOpenGL/PsychGLSLShaders/' filenames ];
    end;
    
    shaderobjs=dir([filenames '*']);
    shaderobjpath = [fileparts([filenames '*']) '/'];
    filenames=[];
    for i=1:size(shaderobjs,1)
        filenames{i} = [shaderobjpath shaderobjs(i).name];
    end;
end;

% Load, compile and attach each single shader of each single file:
for i=1:length(filenames)
    shadername = char(filenames(i));
    shader = LoadShaderFromFile(shadername, [], debug);
    glAttachShader(handle, shader);
end;

% Link the program:
glLinkProgram(handle);

% Ready to use it? Hopefully.
return;
