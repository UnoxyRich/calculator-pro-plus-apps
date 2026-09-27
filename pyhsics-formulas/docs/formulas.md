# PHYS1 formula source

These formulas were transcribed from the attached two-page physics notes. The app uses a consistent axis sign convention: choose the positive direction first, then keep the signs of displacement, velocity, and acceleration consistent.

## Kinematics

- Average speed: total distance / total elapsed time.
- Average velocity: $v = \frac{\Delta x}{\Delta t}$.
- Average acceleration: $a = \frac{\Delta v}{\Delta t}$.
- On graphs, the slope of an $x$-$t$ graph is $v$, the slope of a $v$-$t$ graph is $a$, and the signed area under a $v$-$t$ graph is $\Delta x$.
- $v = v_0 + at$.
- $\Delta x = v_0t + \frac{1}{2}at^2$.
- $v^2 = v_0^2 + 2a\Delta x$.
- $\Delta x = \frac{1}{2}(v+v_0)t$.
- $t = \frac{v-v_0}{a}$.

## Free fall

- $g = 9.8\ \mathrm{m/s^2}$ downward. If upward is positive, $a=-g$.
- Starting from rest: $v_0=0$ and $\Delta x=-\frac{1}{2}gt^2$ when upward is positive.
- At the highest point, $v=0$, $\Delta x=\frac{v_0^2}{2g}$, and $t=\frac{v_0}{g}$.
- Returning to the original ground level: $v=-v_0$ and $t=\frac{2v_0}{g}$.

## Projectile motion

- Horizontal component: $v_{x0}=v_0\cos\theta$ and horizontal velocity is constant.
- Vertical component: $v_{y0}=v_0\sin\theta$ and $a_y=-g$.
- At the highest point, $v_y=0$, $\Delta y=\frac{v_{y0}^2}{2g}$, and $t=\frac{v_{y0}}{g}$.
- Returning to the original ground level: $v_y=-v_{y0}$ and $t=\frac{2v_{y0}}{g}$.
- At a general point, $v=\sqrt{v_x^2+v_y^2}$ and $v_y=v_{y0}-gt$.
- Given $\Delta y$, solve $\Delta y=v_{y0}t-\frac{1}{2}gt^2$ for $t$; discard negative time values.

## Dynamics and the additional exercises

- Weight: $W=mg$. On a horizontal surface at rest, the normal force balances the supported weight.
- Newton's second law: $\sum F_y=ma_y$; at rest, $\sum F_y=0$.
- For stacked boxes at rest, $N_{contact}=m_{top}g$ and $N_{table}=(m_{bottom}+m_{top})g$.
- With downward positive, a falling object that stops over distance $d$ has $a=-v_0^2/(2d)$.
- With downward positive, air resistance upward is $F_{air}=m(g-a)$. For a drop from rest through height $h$, $a=v^2/(2h)$.
- For a package accelerating upward, $T-mg=ma$, so $T=m(g+a)$.
- After a rope is cut when helicopter and package have the same velocity, their separation is $s=L+\frac{1}{2}(a_h+g)t^2$.
- Elevator scale reading $R$ is the normal force: $R-mg=ma$, so $a=(R-mg)/m$.
- Update motion over each constant-acceleration interval with $v_f=v_i+a\Delta t$ and $\Delta x=v_i\Delta t+\frac{1}{2}a(\Delta t)^2$.
- The elevator example uses $m=80\,\mathrm{kg}$ and $g=10\,\mathrm{m/s^2}$; over the four 5-second intervals its accelerations are $0,5,0,-5\,\mathrm{m/s^2}$, end velocities are $0,25,25,0\,\mathrm{m/s}$, and cumulative positions are $0,62.5,187.5,250\,\mathrm{m}$.

The source answer sheet writes $F=ma$ for the torso force from the legs. That equation gives the net force, not the leg force alone. With downward positive, both gravity and the leg force must be included: $mg-F_{legs}=ma$, hence $F_{legs}=m(g-a)$ upward.
