import cv2
import queue
import threading


class VideoCapture:

    def __init__(self, source = 0, max_queue_size=30):
        self.source = source
        self.queue = queue.Queue(maxsize=max_queue_size)
        self.stop_event = threading.Event()
        self.cap = None
        self.thread = None


    def start(self):
        self.cap= cv2.VideoCapture(self.source)
        if not (self.cap.isOpened()):
            raise RuntimeError(f"Could not open Video Source")

        self.thread = threading.Thread(target=self._capture_loop,daemon=True)
        self.thread.start()


    def capture_loop(self):
        while not self.stop_event.is_set():
            ret, frame = self.cap.read()
            if not ret:
                break;

            if self.queue.full():
                try:
                    self.queue.get_nowait()
                except queue.Empty:
                    pass

            self.queue.put(frame.copy())
        
            cv2.imshow('Cam',frame)

        cv2.destroyAllWindows()

    def read_queue(self):
        try:
            return self.queue.get()
        except queue.Empty:
            return None



    def stop(self):
        self.stop_event.set()
        if self.thread:
            self.thread.join()
        if self.cap:
            self.cap.release()

        
