__kernel void paddingAndTransformColorSpace(
        __global unsigned int *rgbaPtr,

        __global unsigned char *yChannel,
        __global unsigned char *cbChannel,
        __global unsigned char *crChannel,

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
    int y, cb, cr;
    r = (value >> 16) & 0xFF;
    g = (value >> 8) & 0xFF;
    b = (value) & 0xFF;

    y = (int) (+0.299f * r + 0.587f * g + 0.114f * b);
    cb = (int) (-0.16874f * r - 0.33126f * g + 0.5f * b + 128.f);
    cr = (int) (+0.5f * r - 0.41869f * g - 0.08131f * b + 128.f);

    yChannel[dstOffset] = y;
    cbChannel[dstOffset] = cb;
    crChannel[dstOffset] = cr;

//    if (posX < 5 && posY < 5)
//        printf("(%d, %d) -> (%d, %d) = #%02x%02x%02x (%02d, %02d, %02d)\n", srcPosX, srcPosY, posX, posY, (int) r,
//               (int) g, (int) b, y, cb, cr);
}
