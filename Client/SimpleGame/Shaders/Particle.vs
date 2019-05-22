#version 330

in vec3 a_Position;
in vec2 a_TexPos;
in vec4 a_Velocity;

uniform vec3 u_TrailDir;
uniform vec4 u_Trans;
uniform float u_ElapsedTime;
uniform float u_MaxTime;

out vec2 v_TexPos;
out float v_Alpha;
out float v_Spark;

void main()
{
	vec4 newPosition;
	newPosition.x = a_Position.x*u_Trans.z;
	newPosition.y = a_Position.y*u_Trans.w;
	newPosition.xy += u_Trans.xy;
	newPosition.z = 0;
	newPosition.w= 1;

	float newTime = u_ElapsedTime - a_Velocity.w;

	vec3 newVelocity = vec3(a_Velocity.x, a_Velocity.y, a_Velocity.z);
	
	if(newTime > 0)
	{
	  newTime = fract(newTime/u_MaxTime) * u_MaxTime;
      newPosition = 
	  vec4(
		  newPosition.x + newVelocity.x*newTime + 0.5*u_TrailDir.x*newTime*newTime,
		  newPosition.y + newVelocity.y*newTime + 0.5*u_TrailDir.y*newTime*newTime,
		  newPosition.z + newVelocity.z*newTime + 0.5*u_TrailDir.z*newTime*newTime,	
		  1.0
		  );
	}
	else
	{
	  newPosition = 
	  vec4(
		  -1000,
		  -1000,
		  -1000,		
		  1.0
		  );
	}

	gl_Position = newPosition;

	v_TexPos = a_TexPos;
	v_Alpha = (u_MaxTime - newTime)/u_MaxTime;
	v_Spark = a_Position.z;
}

