__kernel void paddingAndTransformColorSpace(
        __global unsigned int *rgbaPtr,

        __global float *yChannel,
        __global float *cbChannel,
        __global float *crChannel,

        __global int *otherArguments
) {
    int srcWidth = otherArguments[0];
    int srcHeight = otherArguments[1];
    int dstWidth = otherArguments[2];
    int dstHeight = otherArguments[3];
    int batchOffset = otherArguments[4];
    int groupCnt = otherArguments[5];
    int extraNeedBatchPerFrameX = otherArguments[6];
    int extraNeedBatchPerFrameY = otherArguments[7];


    int srcPosX, srcPosY, posX, posY;
    size_t _x = get_global_id(0);
    size_t _y = get_global_id(1);
    size_t batch = get_global_id(2);

    int extraNeedBatchPerFrame = extraNeedBatchPerFrameX * extraNeedBatchPerFrameY;

    int frameId = batch / extraNeedBatchPerFrame;
    int batchNo = batch % extraNeedBatchPerFrame;
    int blockIdX = batchNo % extraNeedBatchPerFrameX;
    int blockIdY = batchNo / extraNeedBatchPerFrameX;
    posX = batchOffset * blockIdX + _x;
    posY = batchOffset * blockIdY + _y;

    size_t srcOffset;
    size_t dstOffset = posY * dstWidth + posX;

    if (posX >= dstWidth || posY >= dstHeight)
        return;

    if (posX >= srcWidth) {
        srcPosX = srcWidth - 1;
    } else {
        srcPosX = posX;
    }

    if (posY >= srcHeight) {
        srcPosY = srcHeight - 1;
    } else {
        srcPosY = posY;
    }

    srcOffset = srcPosY * srcWidth + srcPosX;
    srcOffset += frameId * srcWidth * srcHeight;
    dstOffset += frameId * dstWidth * dstHeight;

    unsigned int value = (rgbaPtr[srcOffset]);
    float r, g, b;
    float y, cb, cr;
    r = (value >> 16) & 0xFF;
    g = (value >> 8) & 0xFF;
    b = (value) & 0xFF;

    y = (int) (+0.299f * r + 0.587f * g + 0.114f * b) - 128.f;
    cb = (int) (-0.16874f * r - 0.33126f * g + 0.5f * b);
    cr = (int) (+0.5f * r - 0.41869f * g - 0.08131f * b);

    yChannel[dstOffset] = y;
    cbChannel[dstOffset] = cb;
    crChannel[dstOffset] = cr;

//    if (posX < 5 && posY < 5)
//        printf("(%d, %d) -> (%d, %d) = #%02x%02x%02x (%02d, %02d, %02d)\n", srcPosX, srcPosY, posX, posY, (int) r,
//               (int) g, (int) b, y, cb, cr);
}

__kernel void doDCT(
        __global int *otherArguments,
        __global float *channel,
        __global float *outChannel,
        __global float *scaled
) {
    int width = otherArguments[2];
    int height = otherArguments[3];
    size_t _x = get_global_id(0);
    size_t _y = get_global_id(1);
    size_t batch = get_global_id(2);
    size_t offset;
    
    offset = batch * width * height + _x * 8 + _y * 8 * width;

    const float SqrtHalfSqrt = 1.306562965f; //    sqrt((2 + sqrt(2)) / 2) = cos(pi * 1 / 8) * sqrt(2)
    const float InvSqrt      = 0.707106781f; // 1 / sqrt(2)                = cos(pi * 2 / 8)
    const float HalfSqrtSqrt = 0.382683432f; //     sqrt(2 - sqrt(2)) / 2  = cos(pi * 3 / 8)
    const float InvSqrtSqrt  = 0.541196100f; // 1 / sqrt(2 - sqrt(2))      = cos(pi * 3 / 8) * sqrt(2)
    // float block[64] = {-128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128,  -128, -128, -128, -128, -128, -128, -128, -128,  -128, -128, -128, -128, -128, -128, -128, -128,  -128, -128, -128, -128, -128, -128, -128, -128,  -127.174, -127.587, -127.587, -127.587, -128, -128, -128, -128,  -126.174, -126.174, -127.174, -127.587, -127.587, -127.587, -127.587, -127.587,  -125.826, -125.826, -125.826, -126.826, -127.413, -126.826, -127.413, -128};

    float block[64];
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            block[i*8+j] = channel[offset+width*i+j];
        }
    }

    for (int i = 0; i < 8; ++i) {
        // modify in-place
        float* block0 = &block[i*8 + 0    ];
        float* block1 = &block[i*8 + 1 * 1];
        float* block2 = &block[i*8 + 2 * 1];
        float* block3 = &block[i*8 + 3 * 1];
        float* block4 = &block[i*8 + 4 * 1];
        float* block5 = &block[i*8 + 5 * 1];
        float* block6 = &block[i*8 + 6 * 1];
        float* block7 = &block[i*8 + 7 * 1];

        // based on https://dev.w3.org/Amaya/libjpeg/jfdctflt.c , the original variable names can be found in my comments
        float add07 = *block0 + *block7; float sub07 = *block0 - *block7; // tmp0, tmp7
        float add16 = *block1 + *block6; float sub16 = *block1 - *block6; // tmp1, tmp6
        float add25 = *block2 + *block5; float sub25 = *block2 - *block5; // tmp2, tmp5
        float add34 = *block3 + *block4; float sub34 = *block3 - *block4; // tmp3, tmp4

        float add0347 = add07 + add34; float sub07_34 = add07 - add34; // tmp10, tmp13 ("even part" / "phase 2")
        float add1256 = add16 + add25; float sub16_25 = add16 - add25; // tmp11, tmp12

        *block0 = add0347 + add1256; *block4 = add0347 - add1256; // "phase 3"

        float z1 = (sub16_25 + sub07_34) * InvSqrt; // all temporary z-variables kept their original names
        *block2 = sub07_34 + z1; *block6 = sub07_34 - z1; // "phase 5"

        float sub23_45 = sub25 + sub34; // tmp10 ("odd part" / "phase 2")
        float sub12_56 = sub16 + sub25; // tmp11
        float sub01_67 = sub16 + sub07; // tmp12

        float z5 = (sub23_45 - sub01_67) * HalfSqrtSqrt;
        float z2 = sub23_45 * InvSqrtSqrt  + z5;
        float z3 = sub12_56 * InvSqrt;
        float z4 = sub01_67 * SqrtHalfSqrt + z5;
        float z6 = sub07 + z3; // z11 ("phase 5")
        float z7 = sub07 - z3; // z13
        *block1 = z6 + z4; *block7 = z6 - z4; // "phase 6"
        *block5 = z7 + z2; *block3 = z7 - z2;
    }

    for (int i = 0; i < 8; ++i) {
        // modify in-place
        float* block0 = &block[i*1 + 0    ];
        float* block1 = &block[i*1 + 1 * 8];
        float* block2 = &block[i*1 + 2 * 8];
        float* block3 = &block[i*1 + 3 * 8];
        float* block4 = &block[i*1 + 4 * 8];
        float* block5 = &block[i*1 + 5 * 8];
        float* block6 = &block[i*1 + 6 * 8];
        float* block7 = &block[i*1 + 7 * 8];

        // based on https://dev.w3.org/Amaya/libjpeg/jfdctflt.c , the original variable names can be found in my comments
        float add07 = *block0 + *block7; float sub07 = *block0 - *block7; // tmp0, tmp7
        float add16 = *block1 + *block6; float sub16 = *block1 - *block6; // tmp1, tmp6
        float add25 = *block2 + *block5; float sub25 = *block2 - *block5; // tmp2, tmp5
        float add34 = *block3 + *block4; float sub34 = *block3 - *block4; // tmp3, tmp4

        float add0347 = add07 + add34; float sub07_34 = add07 - add34; // tmp10, tmp13 ("even part" / "phase 2")
        float add1256 = add16 + add25; float sub16_25 = add16 - add25; // tmp11, tmp12

        *block0 = add0347 + add1256; *block4 = add0347 - add1256; // "phase 3"

        float z1 = (sub16_25 + sub07_34) * InvSqrt; // all temporary z-variables kept their original names
        *block2 = sub07_34 + z1; *block6 = sub07_34 - z1; // "phase 5"

        float sub23_45 = sub25 + sub34; // tmp10 ("odd part" / "phase 2")
        float sub12_56 = sub16 + sub25; // tmp11
        float sub01_67 = sub16 + sub07; // tmp12

        float z5 = (sub23_45 - sub01_67) * HalfSqrtSqrt;
        float z2 = sub23_45 * InvSqrtSqrt  + z5;
        float z3 = sub12_56 * InvSqrt;
        float z4 = sub01_67 * SqrtHalfSqrt + z5;
        float z6 = sub07 + z3; // z11 ("phase 5")
        float z7 = sub07 - z3; // z13
        *block1 = z6 + z4; *block7 = z6 - z4; // "phase 6"
        *block5 = z7 + z2; *block3 = z7 - z2;
    }

    for(int i = 0; i < 64; ++i) {
        block[i] *= scaled[i];
    }

    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            outChannel[offset+width*i+j] = block[i*8+j];
        }
    }
}