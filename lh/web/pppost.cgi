#!/bin/bash
echo "Content-type: text/plain; charset=ISO-8859-1"
echo "refresh: 30"
echo

cd "`dirname $0`"
echo -n "{oe down ppnext-bus.jsp}{t Brevl�da}{i "
if [[ `cat ../post-in-postbox` = "1" ]]; then
echo  56000000000000000000000000007fff0000000e00000000007fff0000000e00000000007fff0000000e00000000007fff0000000e00000000007fff0000000e00e00000007fff0000000e01e00000007fff0000000e01e00000007fff0000000e03c00000007fff0000000e03800000007fff0000000e07800000007fff0000000e070000000000070000000e0f0000000000070000000e1e0060000000070000000e1c01e000000007000000003807f00000000700000000181fc00000000700000000003f00000000070000000000fe00000000070000000003f000003ffffffffffffe01e000007fffffffffffff81000001fc000700007f1fc0000003e000070000f803e0000003c000070000e000f00000078000070001c00070000007000007000380007800000e000007000380003800000e0000070007ffffffffe01e0000070007ffffffffe01c0000070007ffffffffe01c000007000700000000e01c000007800600000000e01c00000fe00600000000e01c00001ff00e00000000e01c00003c700e03fff000e01c000038780e03fff0f8e01c000070380e03fff0f8e01c000070380e000000f8e01c000070780e000000f0e01c000038f00e03fff0f0e01c00003ff00e03fff000e01c00001fe00e03fff000e01c000003000e00000000e01c000000000e00080000e01c000000000e03f80000e01c000000000e03f80000e01c000000000e00000000e01c000000000e00000000e01c000000000e00000000e01c000000000e00000000e01c000000000e00000000e01c000000000f00000000e01fffffffffffffffffffe01fffffffffffffffffffe003fff1ffffe700001ef7a0000001ffff0300000e0000000001ffff0380000e0000000001ffff038000070000000001dfff01c000070000000001c7ff01c000038000000001c1ff01e000038000000001c07f00f000038000000001c01f00f000038000000001c00f00780003c000000001c007003c0003c000000001c007003e0003c000000001c007001f00038000000001c007000f80078000000001c0070007c00f8000000001c0070001ffff8000000001ffff0000fffe0000000001ffff00001ff80000000001ffff000000000000000000fffe}
else
echo  560000000000000000000000001fffffffffffff000000003fffffffffffffc0000000fe000000003f0fe0000001f0000000007c01f0000001c000000000700078000003c000000000e000380000038000000001c0003c0000070000000001c0000c0000070000007803c0000e00000f000001fe0380000e00000e000003ff0380000e00000e000003cf0380000e000000000007070300000e00007fffffff038300000e00007fffffff038700000e00007fffffff038700000e00007ff00003870700000e00007ff00003ff0700000e00007ff00000fe0700000e00007ff00000380700000e0000fff00000000700000e00007ff00000000700000e00007ff00000000700000e00007ff00000000700000e00007ff00000000700000e0000fff00000000700000e00007ff00000000700000e00007ff00000000700000e00000e000000000700000e00001e000000000700000e00000e000000000700000e00000e000000000700000e00000e000000000780000e00000ffffffffffffffffe00000ffffffffffffffffe0000038000fffff383fffe0000000000ffff818000070000000000ffff81c000078000000000ffff81c000038000000000efff80e000038000000000e3ff80e00001c000000000e0ff80f00001c000000000e07f80700001c000000000e01f80780001c000000000e007803c0001e000000000e003801e0001e000000000e003801f0001c000000000e003800f0001c000000000e0038007c003c000000000e0038003e007c000000000e0038000ffffc000000000ffff80007fff0000000000ffff80000ffe0000000000ffff8000000000000000007fff80000000000000000000000000000000}
fi
echo
echo Temperatur
echo "  inne" `../get 100EE90700000047` grader C
echo "  ute" `../get 10E4790600000099` grader C
echo
EOF
if ../get 05F3A70200000087 && ../get 057CAC160000000F && ../get 0596B11600000079; then
	# its locked
	echo "L�st"
else
	# its not locked
	echo -n "Ej l�st i"
	../get 05F3A70200000087 || echo -n " datarum"
	../get 057CAC160000000F || echo -n " garage"
	../get 0596B11600000079 || echo -n " entr�"
	echo
fi
../get 0549A40200000001 && echo Motorv�rmaren �r p�