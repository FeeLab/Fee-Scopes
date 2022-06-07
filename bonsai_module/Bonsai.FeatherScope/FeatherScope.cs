#undef ALL_UVC_PROPERTIES

using System;
using OpenCV.Net;
using System.Reactive.Linq;
using System.ComponentModel;
using System.Threading.Tasks;
using System.Drawing.Design;

namespace Bonsai.FeatherScope
{
    // Attribute metadata. Description attributes specify strings that will be included as
    // documentation in the editor
    [Description("Produces a video sequence of images acquired from a UCLA Miniscope.")]
    public class FeatherScope : Source<IplImage>
    {
        // Only one exclusive connection can be made to a hardware camera object. However, many
        // observers may want to connect to the camera. Because of this, the strategy for the
        // CameraCapture node is to create and share the single connection between all observers.
        // When the first observer subscribes to the source, the connection is created. When the
        // last observer unsubscribes, the connection is destroyed.
        // More details below.
        IObservable<IplImage> source;

        // This lock is only necessary because of asynchronous creation and destruction of the
        // camera connection. In case we are rapidly creating and destroying camera connections
        // we want to wait until the last connection is destroyed before we create a new one.
        // This lock ensures that.
        readonly object captureLock = new object();

#if ALL_UVC_PROPERTIES
        // Properties of the camera. This is mostly OpenCV details.
        readonly CapturePropertyCollection captureProperties = new CapturePropertyCollection();
#endif

        // Functor
        public FeatherScope()
        {
#if !ALL_UVC_PROPERTIES
            lastSensorGain = SensorGain;
#endif

            // Here we define our observable source. An observable is just a (possibly asynchronous)
            // sequence of events that gets propagated to an arbitrary number of downstream observers.
            // You can think about it as an iterator/generator/list/collection (pick your poison)
            // with the difference that instead of consumers having to pull out items from it, they
            // get pushed as notifications to each observer.
            //
            // One important point is that observable sequences only actually need to do something
            // when an observer subscribes to it.
            //
            // The Observable.Create method makes it easy to create one of these sequences, by simply
            // specifying a function that gets called for each observer. Inside this function, you can
            // send notifications to the observer by calling observer.OnNext(). Also, you need to specify
            // what happens if the observer cancels the subscription at any time (e.g. the workflow is
            // stopped). There are many possible overloads to Observable.Create, but in this case, we
            // are defining the sequence in terms of a Task that starts and stops tied to a particular
            // observer subscription. The cancellationToken variable allows us to be notified when the
            // observer cancelled the subscription.
            source = Observable.Create<IplImage>((observer, cancellationToken) =>
            {
                // Here we simply start the task that will emit the notifications to the observer
                return Task.Factory.StartNew(() =>
                {
                    // We wait until any previous connections are completely disposed.
                    lock (captureLock)
                    {
                        // We create the camera connection
                        using (var capture = Capture.CreateCameraCapture(Index))
                        {

#if ALL_UVC_PROPERTIES
                            // Apply the settings
                            foreach (var setting in captureProperties)
                            {
                                capture.SetProperty(setting.Property, setting.Value);
                            }
                            captureProperties.Capture = capture;
#else
                            capture.SetProperty(CaptureProperty.Gain, SensorGain);
#endif
                            try
                            {
                                // Loop until the observer has cancelled the subscription
                                while (!cancellationToken.IsCancellationRequested)
                                {
#if ALL_UVC_PROPERTIES
                                    // Read one image
                                    var image = captureProperties.Capture.QueryFrame();
#else
                                    // Runtime settable properties
                                    if (SensorGain != lastSensorGain)
                                    {
                                        capture.SetProperty(CaptureProperty.Gain, SensorGain);
                                        lastSensorGain = SensorGain;
                                    }

                                    var image = capture.QueryFrame();
#endif

                                    if (image == null)
                                    {
                                        // If the next image is null, the camera was somehow stopped,
                                        // so we signal the observer that the sequence has ended.
                                        // This mostly never happens, but just to be sure.
                                        observer.OnCompleted();
                                        break;
                                    }
                                    // Otherwise, send a copy of the image to the observer. The reason we
                                    // send a copy is that acquisition of the next frame will overwrite the
                                    // original image; this is a problem if the observer cached the image
                                    // somewhere for future use.
                                    else observer.OnNext(image.Clone());
                                }
                            }
                            // Make sure we reset the capture property to null at the end
                            finally
                            {
#if ALL_UVC_PROPERTIES
                                captureProperties.Capture = null;
#endif
                                capture.Close(); // necessary? Not in Goncalo's example
                            }

                        }
                    }
                },
                // These next parameters specify the operation of the Task. We give it the token, so that
                // the task is cancelled in case the observer unsubscribes. We also indicate the Task is long
                // running so that the framework allocates a dedicated thread to it, rather than a worker thread
                // from the thread pool. The last parameter just assigns it the default scheduler.
                cancellationToken,
                TaskCreationOptions.LongRunning,
                TaskScheduler.Default);
            })
            // The next two methods make this source a shared (hot) source. This ensures there is only one observer
            // subscribed to the camera and all notifications are distributed among all other observers.
            // RefCount ensures that the connection is only made when there are actually observers.
            .PublishReconnectable()
            .RefCount();
        }

        // Settings
        [Description("The index of the camera from which to acquire images.")]
        public int Index { get; set; } = 0;

#if ALL_UVC_PROPERTIES
        // Properties for the property window
        [Description("Specifies the set of capture properties assigned to the camera.")]
        public CapturePropertyCollection CaptureProperties
        {
            get { return captureProperties; }
        }
#endif
        [Range(1, 3)]
        [Editor(DesignTypes.SliderEditor, typeof(UITypeEditor))]
        [Description("The sensor gain, 2 has best SNR, 3 is actually 3.5x.")]
        public int SensorGain { get; set; } = 2;

        private int lastSensorGain;

        // Since we have defined our observable source in the constructor (because we are sharing it), here we
        // just need to return that source.
        public override IObservable<IplImage> Generate()
        {
            return source;
        }
    }
}
