# ESP32 Spectrum Analyzer using FFT

## Components

- **Beckend database**: Supabase
- **WebApp**: Streamlit
- **Wifi-based communication between ESP32 and PC**: Websocket client-server with simple json/text messages

## How to run

First, you need to prepare the hardware setup according to the scheme and upload the code to ESP32. Then, you need to run the Streamlit app and connect to the ESP32. The app will automatically connect to the Supabase database and start receiving data from ESP32. The app will also start the FFT analysis and display the results.

### Step-by-step guide

Upload the code to ESP32 using Arduino, or any other method you prefer. Pay attention on the problems with the `analogRead` function described in the documentation. Use the sketch in sketch_sample_rate do identify your sample rate and the maximum frequency you can sample.

Edit the .secrets.toml and secrets.h files in .streamlit and sketch_fft files respectively. You need to fill the Supabase API data and wifi adress + password.

Create a docker image of the Streamlit app using the `Dockerfile`:

```bash
# Build the docker image using the Dockerfile
docker build -t streamlit .
# Run the docker container using the image
docker run -p 8501:8501 streamlit
```

After that you can use the IP address of the app, which is displayed in the terminal of the container, to connect to the app using your browser.

Then you can use the GUI to connect to the ESP32 and start the FFT analysis. It is also possible to connect to the Supabase database and display the data from the ESP32.
