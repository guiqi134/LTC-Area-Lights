# Real-time LTC Area Lights
This project is based on the paper "Real-time polygonal-light shading with linearly transformed cosines" in 2017. A total of 4 area light types are
implemented in the project using LTC method. 
  
Project Report: [Google Doc Link](https://docs.google.com/document/d/1HdXiNwrHgoKwy6XaQvOW3WtLvpJrJ3mPKpNqYA6fmIg/edit)  
Project Video: [Youtube Link](https://youtu.be/2Uah_THLpQ8)

## Gallary
![](https://github.com/guiqi134/LTC-Area-Lights/blob/master/images/image1.jpg?raw=true)
![](https://github.com/guiqi134/LTC-Area-Lights/blob/master/images/image2.jpg?raw=true)
![](https://github.com/guiqi134/LTC-Area-Lights/blob/master/images/image3.jpg?raw=true)
![](https://github.com/guiqi134/LTC-Area-Lights/blob/master/images/image4.jpg?raw=true)
![](https://github.com/guiqi134/LTC-Area-Lights/blob/master/images/image5.jpg?raw=true)
![](https://github.com/guiqi134/LTC-Area-Lights/blob/master/images/image6.jpg?raw=true)

## References
- **Linear Transformed Cosines(LTC)**: [Real-time polygonal-light shading with linearly transformed cosines](https://eheitzresearch.wordpress.com/415-2/)  
- **Rectangle light**: [Real-Time Area Lighting: a Journey from Research to Production](https://blog.selfshadow.com/publications/s2016-advances/s2016_ltc_rnd.pdf)  
- **Cylinder light**: [Linear-Light Shading with Linearly Transformed Cosines - Unity Technologies Blog](https://blogs.unity3d.com/2017/04/17/linear-light-shading-with-linearly-transformed-cosines/)  
- **Disk light**: [Real-Time Line- and Disk-Light Shading with Linearly Transformed Cosines](https://blog.selfshadow.com/publications/s2017-shading-course/heitz/s2017_pbs_ltc_lines_disks.pdf)  
- **Sphere light**: [Analytical Calculation of the Solid Angle Subtended by an Arbitrarily Positioned Ellipsoid to a Point Source - Unity Technologies Blog](https://blogs.unity3d.com/cn/2017/02/08/analytical-calculation-of-the-solid-angle-subtended-by-an-arbitrarily-positioned-ellipsoid-to-a-point-source/)

## Dependence
Library                                     | Description
------------------------------------------  | -------------
[glad](https://github.com/Dav1dde/glad)     | OpenGL Loader
[assimp](https://github.com/assimp/assimp)  | Model loading
[glfw](https://github.com/glfw/glfw)        | Windowing And Input Handling
[glm](https://github.com/g-truc/glm)        | Matrix operation
[imgui](https://github.com/ocornut/imgui)   | UI drawing
[Eigen](https://eigen.tuxfamily.org/index.php?title=Main_Page) | Eigenvalue decomposition


## TODO
1. Since OpenGL doesn’t support ray tracing, I plan to migrate this project later into Falcor and use DXR to calculate the soft shadows for those area lights  
2. Inserting more area light types like star, cubic, torus, etc  
3. When using normal mapping and transferring all the shading data into tangent space, it would cause mismatching problems in the LTC lookup table. So, I have to transform all normals into world space in the fragment shader which is not a good choice  
4. I just displace the vertex position in a ripple effect. It would be better to reconstruct the normals for those new positions
 

