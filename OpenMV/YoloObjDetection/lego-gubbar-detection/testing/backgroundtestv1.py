import cv2
import numpy as np

def nothing(x):
    pass

cv2.namedWindow('filters')

cv2.createTrackbar('cannyLow','filters',0,255,nothing)
cv2.createTrackbar('cannyHigh','filters',55,255,nothing)
cv2.createTrackbar('blur','filters',3,255,nothing)
cv2.createTrackbar('diolate','filters',4,255,nothing)
cv2.createTrackbar('errode','filters',7,255,nothing)

#== Parameters =======================================================================
# BLUR = 21
# CANNY_THRESH_1 = 10
# CANNY_THRESH_2 = 200
# MASK_DILATE_ITER = 10
# MASK_ERODE_ITER = 10
MASK_COLOR = (0.0,0.0,1.0) # In BGR format


#== Processing =======================================================================

#-- Read image -----------------------------------------------------------------------
img = cv2.imread('GetImages/Images/test.png')
gray = cv2.cvtColor(img,cv2.COLOR_BGR2GRAY)

while True:

    BLUR = int(cv2.getTrackbarPos('blur','filters'))  # Hue minimum
    CANNY_THRESH_1 = int(cv2.getTrackbarPos('cannyLow','filters'))  # Saturation minimum
    CANNY_THRESH_2 = int(cv2.getTrackbarPos('cannyHigh','filters'))   # Value minimum (Also referred to as brightness)
    MASK_DILATE_ITER = int(cv2.getTrackbarPos('diolate','filters')) # Hue maximum
    MASK_ERODE_ITER = int(cv2.getTrackbarPos('errode','filters')) # Saturation maximum
    
    #-- Edge detection -------------------------------------------------------------------
    edges = cv2.Canny(gray, CANNY_THRESH_1, CANNY_THRESH_2)
    edges = cv2.dilate(edges, None)
    edges = cv2.erode(edges, None)

    #-- Find contours in edges, sort by area ---------------------------------------------
    contour_info = []
    contours, _ = cv2.findContours(edges, cv2.RETR_LIST, cv2.CHAIN_APPROX_NONE)
    # Previously, for a previous version of cv2, this line was: 
    #  contours, _ = cv2.findContours(edges, cv2.RETR_LIST, cv2.CHAIN_APPROX_NONE)
    # Thanks to notes from commenters, I've updated the code but left this note
    for c in contours:
        contour_info.append((
            c,
            cv2.isContourConvex(c),
            cv2.contourArea(c),
        ))
    contour_info = sorted(contour_info, key=lambda c: c[2], reverse=True)
    max_contour = contour_info[0]

    #-- Create empty mask, draw filled polygon on it corresponding to largest contour ----
    # Mask is black, polygon is white
    mask = np.zeros(edges.shape)
    cv2.fillConvexPoly(mask, max_contour[0], (255))

    #-- Smooth mask, then blur it --------------------------------------------------------
    mask = cv2.dilate(mask, None, iterations=MASK_DILATE_ITER)
    mask = cv2.erode(mask, None, iterations=MASK_ERODE_ITER)
    mask = cv2.GaussianBlur(mask, (BLUR, BLUR), 0)
    mask_stack = np.dstack([mask]*3)    # Create 3-channel alpha mask

    #-- Blend masked img into MASK_COLOR background --------------------------------------
    mask_stack  = mask_stack.astype('float32') / 255.0          # Use float matrices, 
    newimg         = img.astype('float32') / 255.0                 #  for easy blending

    masked = (mask_stack * newimg) + ((1-mask_stack) * MASK_COLOR) # Blend
    masked = (masked * 255).astype('uint8')                     # Convert back to 8-bit 

    cv2.imshow('img', masked)                                   # Display
    cv2.waitKey()
