function [u,uexact,error] = rb(N,tol)
% Red black solver
%       n is size of interval
%       tol  error tolerance
%       u is the computed solution
%       u is the exact solution
%       error is the exact error

u=[1:N];
u(1:N)=zeros(N,1);
u(2:N-1)=ones(N-2,1);

% Perform iterations until convergence
% a tolerance  tol.
h=1.0/(N-1);
h
x=[0:N-1];
x=x.*h;
uexact = 4*x.*(x-1);
err=100*tol;
i=0;
while(err >tol )
   i=i+1;
   u(2:2:N-1) = (u(1:2:N-2) + u(3:2:N) - 8*h*h)/2.0;
   u(3:2:N-1) = (u(2:2:N-2) + u(4:2:N) - 8*h*h)/2.0;
%   if rem(i,20) == 0
   if rem(i,N) == 0
       err = max(abs(u-uexact));
       fprintf('%d %6f \n',i,err);
%       plot(u)
%       pause
   end
end
fprintf('# iterations: %d\n',i+1);

error = max(abs(u-uexact));
plot(u);
hold;
plot(uexact,'r');
hold off;
