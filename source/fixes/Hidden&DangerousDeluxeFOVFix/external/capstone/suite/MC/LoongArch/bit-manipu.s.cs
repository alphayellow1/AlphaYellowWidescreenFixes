# CS_ARCH_LOONGARCH, CS_MODE_LOONGARCH64, None
0x61,0x10,0x00,0x00 = clo.w $ra, $sp
0x47,0x15,0x00,0x00 = clz.w $a3, $a6
0xc2,0x18,0x00,0x00 = cto.w $tp, $a2
0xc5,0x1e,0x00,0x00 = ctz.w $a1, $fp
0x1d,0x40,0x08,0x00 = bytepick.w $s6, $zero, $t4, 0
0x74,0x31,0x00,0x00 = revb.2h $t8, $a7
0x75,0x4b,0x00,0x00 = bitrev.4b $r21, $s4
0xb9,0x50,0x00,0x00 = bitrev.w $s2, $a1
0x68,0x09,0x67,0x00 = bstrins.w $a4, $a7, 7, 2
0x21,0x91,0x6a,0x00 = bstrpick.w $ra, $a5, 0xa, 4
0x74,0x49,0x13,0x00 = maskeqz $t8, $a7, $t6
0xb4,0xe9,0x13,0x00 = masknez $t8, $t1, $s3