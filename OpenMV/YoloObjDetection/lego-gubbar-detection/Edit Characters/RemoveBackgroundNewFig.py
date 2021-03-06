# ANVÄND LILA BAKGRUND

import cv2
import numpy as np
from PIL import Image
import glob
import os

MASK_COLOR = (0,0,0,0)

pixlThreshold = 1500
skipamount = 10

hMin = 0
sMin = 160
vMin = 0
hMax = 255
sMax = 255
vMax = 255
MASK_DILATE_ITER = 9
MASK_ERODE_ITER = 8
BLUR = 9
KERNEL = np.ones((9,9), np.uint8)

lower = np.array([hMin, sMin, vMin])
upper = np.array([hMax, sMax, vMax])
total = 0

for video in glob.glob("GetImages/VideoDone/*.mp4"):
    cap = cv2.VideoCapture(video)
    skipframe = 0
    while(cap.isOpened()):
        try:
            ret, img = cap.read()
            if not ret:
                break
            skipframe += 1 
            if skipframe <= skipamount:
                continue
            skipframe = 0

            hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)

            mask = cv2.inRange(hsv, lower, upper)
            mask = cv2.dilate(mask, None, iterations=MASK_DILATE_ITER)
            mask = cv2.erode(mask, None, iterations=MASK_ERODE_ITER)
            mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, KERNEL, iterations=2)
            mask = cv2.GaussianBlur(mask, (BLUR, BLUR), 0)

            edges = cv2.Canny(mask, 0, 255)
            edges = cv2.dilate(edges, None)
            edges = cv2.erode(edges, None)

            contour_info = []
            contours, _ = cv2.findContours(edges, cv2.RETR_LIST, cv2.CHAIN_APPROX_NONE)

            for c in contours:
                if cv2.contourArea(c) < pixlThreshold:
                    continue
                contour_info.append((
                    c,
                    cv2.isContourConvex(c),
                    cv2.contourArea(c),
                ))

            skip = True

            for contour in contour_info:
                if skip:
                    skip = False
                    continue
                mask = np.zeros(edges.shape)
                cv2.drawContours(mask, [contour[0]],-1, 255, -1)

                mask_stack = np.dstack([mask]*4)
                mask_stack  = mask_stack.astype('float') / 255.0 
                newimg = cv2.multiply(mask_stack, cv2.cvtColor(img, cv2.COLOR_BGR2BGRA).astype("float")/255)
                cords = Image.fromarray(newimg.astype('uint8'), 'RGBA').getbbox()
                newimg = newimg[cords[1]:cords[3],cords[0]:cords[2]]

                cv2.imwrite("Edit Characters/FramesNoBackground/"+"{}.png".format(str(total).zfill(8)), (newimg * 255).astype('uint8'))
                total+=1
                skip = True
        except:
            pass
    cap.release()