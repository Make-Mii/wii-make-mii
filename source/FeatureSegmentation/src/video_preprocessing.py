import cv2
from collections import deque


cam = cv2.VideoCapture(0)


frame_width = int(cam.get(cv2.CAP_PROP_FRAME_WIDTH))
frame_height = int(cam.get(cv2.CAP_PROP_FRAME_HEIGHT))


#circular buffer to store frames of video 
BUFFER_SIZE = 30
frame_buffer = deque(maxlen=BUFFER_SIZE)




while True:
    ret, frame = cam.read()

    if ret:
        frame_buffer.append(frame.copy())


    cv2.imshow('Cam',frame)

    # Press 'q' to exit the loop
    if cv2.waitKey(1) == ord('q'):
        break



# Release the capture and writer objects
cam.release()
cv2.destroyAllWindows()
