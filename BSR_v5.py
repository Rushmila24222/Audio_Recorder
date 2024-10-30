
# In[1]:
# install the required libraries using cmd 
#python -m pip install pyaudio sounddevice numpy wave wavio boto3 requests

# In[1]:
#import libraries 
import sounddevice as sd
import platform
import uuid
import numpy as np
import wavio
from datetime import datetime, timezone
import time
import os
import boto3
import json
import requests
import urllib.request
import base64

# In[4]:
#aws_credentials 
os.environ['AWS_ACCESS_KEY_ID'] = '******'
os.environ['AWS_SECRET_ACCESS_KEY'] = '***************'
os.environ['AWS_DEFAULT_REGION'] = '********'

# In[5]:
# Define S3 bucket and folders
s3_bucket_name = "nicatsaudio"
audio_folder = "storingaudios"
transcription_folder = "audio2text"

# In[8]:
computer_name = platform.node()
c_id = uuid.getnode()
computer = uuid.UUID(int=uuid.getnode())
print(computer)
print(c_id)

# In[13]:
# List all audio devices to find the correct input device index
def get_device_list():
    devices = sd.query_devices()
    device_list = [f"{i} {device['name']}, {device['hostapi']} ({device['max_input_channels']} in, {device['max_output_channels']} out)" for i, device in enumerate(devices)]
    for i, device in enumerate(devices):
        if "Stereo Mix" in device['name']: 
            index = i;        
    return index;
print(sd.query_devices())
index =get_device_list()


# In[9]:
#Method for recording the background audio
def record_audio(duration, filename,timestamp, output_dir, device_index=None):
    
    fs = 48000 # Sample rate
    channels = 2  # Stereo

    print("Recording audio...")

    # Record audio from specified input device
    recording = sd.rec(int(duration * fs), samplerate=fs, channels=channels, dtype='int16', device=device_index)
    sd.wait()  # Wait until recording is finished

    print("Recording finished.")

    # To check the output directory exists
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    # Capture the end time 
    end_time = datetime.utcnow().strftime("%Y-%m-%d_%H:%M:%S")
    
    #computer_name = platform.node()

    # Save the recorded audio with computer name and timestamp
    filename_with_timestamp = f"{computer_name}_{filename}_{timestamp}.wav"
    filepath = os.path.join(output_dir, filename_with_timestamp)

    # Write the audio data to a WAV file
    wavio.write(filepath, recording, fs, sampwidth=2)

    print(f"Audio recorded and saved as '{filename_with_timestamp}'.")
    
    # Encode the audio file in Base64
    audio_file =open(filepath, "rb")
    encoded_audio = base64.b64encode(audio_file.read()).decode('utf-8')

    return filepath, encoded_audio, end_time

#Method for uploading audio file in the S3 bucket
def upload_audio_to_s3(file_path, bucket_name, object_name):
    s3_client = boto3.client('s3')

    try:
        s3_client.upload_file(file_path, bucket_name, object_name)
        print(f"File '{file_path}' uploaded to S3 bucket '{bucket_name}' as '{object_name}'.")
        return object_name  
    except Exception as e:
        print(f"Failed to upload {file_path} to S3: {e}")
        return None

#Method for transcribe audio in aws
def transcribe_audio(file_uri, job_name, region_name="us-east-2"):
    transcribe = boto3.client('transcribe', region_name=region_name)

    transcribe.start_transcription_job(
        TranscriptionJobName=job_name,
        Media={'MediaFileUri': file_uri},
        MediaFormat='wav',  
        LanguageCode='en-US'
    )

    while True:
        status = transcribe.get_transcription_job(TranscriptionJobName=job_name)
        if status['TranscriptionJob']['TranscriptionJobStatus'] in ['COMPLETED', 'FAILED']:
            print("Transcription complete")
            break
        
    if status['TranscriptionJob']['TranscriptionJobStatus'] == 'COMPLETED':
        transcription_file_uri = status['TranscriptionJob']['Transcript']['TranscriptFileUri']
        return transcription_file_uri
    else:
        print("Transcription failed")
        return None    

# Method for downloading the transcribed file from url and storing to local device
def download(url, filename, timestamp, output_dir):
    s3 = boto3.client('s3')
    # To check the output directory exists
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
        
    #computer_name = platform.node()
    filename = f"{computer_name}_{filename}_{timestamp}.json"
    #print(type(filename))
    filepath = os.path.join(output_dir, filename)
    
    urllib.request.urlretrieve(url,filepath)
    
    print(f"File downloaded and saved as '{filename}'.")

    return filepath

#Method for uploading transcribed file in the S3 bucket
def upload_json_to_s3(file_path, bucket_name, object_name):
    s3_client = boto3.client('s3')

    try:
        s3_client.upload_file(file_path, bucket_name, object_name)
        print(f"File '{file_path}' uploaded to S3 bucket '{bucket_name}' as '{object_name}'.")
        return object_name  
    except Exception as e:
        print(f"Failed to upload {file_path} to S3: {e}")
        return None

#Load data from JSON file
#file_path = downloaded_file_path.replace('/', "\\")

def load_json(file_path):
    f = open(file_path)
    data = json.load(f)
    # Extract the items
    items = data['results']['items']
    #List to store the extracted information in a simplified way
    item_list = []
    item_start_time_list = []
    item_end_time_list = []
     
    # Process and print the item number with content, start time, and end time
    for i, item in enumerate(items):
        content = item['alternatives'][0]['content'] if 'alternatives' in item and item['alternatives'] else 'N/A'
        start_time = item['start_time'] if 'start_time' in item else 'N/A'
        end_time = item['end_time'] if 'end_time' in item else 'N/A'
        print(f"Item {i+1}: {content} , start_time={start_time}, end_time={end_time}")
        
        item_list.append(content)
        item_start_time_list.append(start_time)
        item_end_time_list.append(end_time)
        
    f.close()
    return item_list, item_start_time_list, item_end_time_list