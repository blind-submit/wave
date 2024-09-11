#include <iostream>

#include "dpf.hpp"

int main(int argc, char * argv[])
{
    int a = dpf::uniform_sample<int>(), x = dpf::uniform_sample<int>(), u = dpf::uniform_sample<int>(), b = dpf::uniform_sample<int>();
    int A = dpf::uniform_sample<int>();
    int X = dpf::uniform_sample<int>();
    int U = dpf::uniform_sample<int>();
    int B = dpf::uniform_sample<int>();

    int aa = a+A;
    int xx = x+X;
    int uu = u+U;
    int bb = b+B;

    int AXmB = A*X-B;
    int UX = U*X;
    int UAXmB = U*AXmB;
    int UA = U*A;

    auto [a0, a1] = dpf::additively_share(a);
    auto [x0, x1] = dpf::additively_share(x);
    auto [u0, u1] = dpf::additively_share(u);
    auto [b0, b1] = dpf::additively_share(b);

    auto [A0, A1] = dpf::additively_share(A);
    auto [X0, X1] = dpf::additively_share(X);
    auto [U0, U1] = dpf::additively_share(U);
    auto [B0, B1] = dpf::additively_share(B);

    auto [UA0, UA1] = dpf::additively_share(UA);
    auto [UX0, UX1] = dpf::additively_share(UX);
    auto [AXmB0, AXmB1] = dpf::additively_share(AXmB);
    auto [UAXmB0, UAXmB1] = dpf::additively_share(UAXmB);

    int y0 = uu*((aa*xx+bb) - aa*X0 - xx*A0 + AXmB0) - (aa*xx+bb)*U0 + aa*UX0 + xx*UA0 - UAXmB0;
    int y1 = uu*(           - aa*X1 - xx*A1 + AXmB1) - (aa*xx+bb)*U1 + aa*UX1 + xx*UA1 - UAXmB1;

    std::cout << (y0+y1) << " =?= " << u*(a*x+b) << "\n";
    return 0;
}
