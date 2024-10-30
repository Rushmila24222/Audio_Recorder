#include <Python.h>
#include <iostream>
#include <algorithm> 
#include <ctime>
#include <string>
#include <filesystem>
#include <vector>

int main()
{
    // Current time in (YYYYMMDD_HHMMSS) format 
    std::time_t time = std::time(nullptr);
    char timestamp[20];
    std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", std::gmtime(&time));

    // Define S3 bucket and folders
    std::string s3_bucket_name = "nicatsaudio";
    std::string audio_folder = "storingaudios";
    std::string transcription_folder = "audio2text";
    
    //Python Interpreter
    Py_Initialize();

    PyObject *name, *load_module, *func, *callfunc, *args;
    name = PyUnicode_FromString((char*)"BSR_v5");
    load_module = PyImport_Import(name);

    // Device index
    //std::vector<std::string> deviceList;
    func = PyObject_GetAttrString(load_module,(char*)"get_device_list");
    callfunc = PyObject_CallObject(func,NULL);
    int deviceIndex = PyLong_AsLong(callfunc);
    std::cout << "Device index: " << deviceIndex << std::endl;

    //Function 1: record_audio(duration, filename, timestamp, output_dir, device_index)
    func = PyObject_GetAttrString(load_module,(char*)"record_audio");
    args = PyTuple_Pack(5, PyLong_FromLong(6),                                // duration (6 seconds)
                           PyUnicode_FromString("output"),                             // filename
                           PyUnicode_FromString(timestamp),                            // timestamp
                           PyUnicode_FromString("C:/Users/Public/Music/AudioFiles"),  // output_dir
                           PyLong_FromLong(deviceIndex));                            //device index
    callfunc = PyObject_CallObject(func,args);
    std::string recordedFilePath = PyUnicode_AsUTF8(PyTuple_GetItem(callfunc, 0));
    std::string encoded_audio = PyUnicode_AsUTF8(PyTuple_GetItem(callfunc, 1));
    std::string recordingEndTime = PyUnicode_AsUTF8(PyTuple_GetItem(callfunc, 2));

    //Function 2: upload_audio_to_s3(file_path, bucket_name, object_name)
    std::string objectName = std::filesystem::path(recordedFilePath).filename().string(); //object name
    func = PyObject_GetAttrString(load_module,(char*)"upload_audio_to_s3");
    args = PyTuple_Pack(3, PyUnicode_FromString(recordedFilePath.c_str()),                 // filepath(result stored in a variable)
                           PyUnicode_FromString(s3_bucket_name.c_str()),                   // bucket_name
                           PyUnicode_FromString((audio_folder +"/"+objectName).c_str())); // object_name                                    
    callfunc = PyObject_CallObject(func,args);
    const char* object_name = PyUnicode_AsUTF8(callfunc);
    std::string uploadedAudio = "s3://"+ s3_bucket_name + "/" + object_name;

    //Function 3: transcribe_audio(file_uri, job_name, region_name="us-east-2")
    std::string job_name = "transcription_job_"+objectName;
    func = PyObject_GetAttrString(load_module,(char*)"transcribe_audio");
    args = PyTuple_Pack(3, PyUnicode_FromString( uploadedAudio.c_str()), // file_uri(result stored in uploadedObject variable)
                           PyUnicode_FromString(job_name.c_str()),      // bucket_name
                           PyUnicode_FromString("us-east-2"));         //aws region
    callfunc = PyObject_CallObject(func,args);
    std::string transcriptionURI = PyUnicode_AsUTF8(callfunc);

    //Function 4: download(url, filename, timestamp, output_dir)
    func = PyObject_GetAttrString(load_module,(char*)"download");
    args = PyTuple_Pack(4, PyUnicode_FromString(transcriptionURI.c_str()),                    // URL of the transcribed file in aws
                           PyUnicode_FromString("output"),                                   // filename
                           PyUnicode_FromString(timestamp),                                  // timestamp
                           PyUnicode_FromString("C:/Users/Public/Music/TranscribedAudio")); // download_dir;
    callfunc = PyObject_CallObject(func,args);
    std::string downloadedFilePath = PyUnicode_AsUTF8(callfunc);

    //Function 5: upload_json_to_s3(file_path, bucket_name, object_name)
    std::string jobjectName = std::filesystem::path(downloadedFilePath).filename().string(); //object name
    func = PyObject_GetAttrString(load_module,(char*)"upload_json_to_s3");
    args = PyTuple_Pack(3, PyUnicode_FromString(recordedFilePath.c_str()),                             // filepath(result stored in a variable)
                           PyUnicode_FromString(s3_bucket_name.c_str()),                              // bucket_name
                           PyUnicode_FromString((transcription_folder +"/"+jobjectName).c_str()));   // object_name                                   
    callfunc = PyObject_CallObject(func,args);
    const char* jobject_name = PyUnicode_AsUTF8(callfunc);
    std::string uploadedJSON = "s3://"+ s3_bucket_name + "/" + jobject_name;

    //Load data from JSON file: load_json(file_path)
    std::vector<std::string> itemList;
    std::vector<std::string> itemstList;
    std::vector<std::string> itemetList; 
    std::string file_path = downloadedFilePath;
    std::replace(file_path.begin(), file_path.end(), '/', '\\'); //file_path
    func = PyObject_GetAttrString(load_module,(char*)"load_json");
    args = PyTuple_Pack(1, PyUnicode_FromString(file_path.c_str()));   // filepath(result stored in a variable)                                  
    callfunc = PyObject_CallObject(func,args);
    std::cout << PyList_Size(callfunc) << std::endl;
    int length = PyList_Size(callfunc);
    for (int i=0;i<PyList_Size(callfunc);i++){
        PyObject *pItem = PyList_GetItem(PyTuple_GetItem(callfunc, 0), i);
        itemList.push_back(PyUnicode_AsUTF8(pItem));
        PyObject *pStartTime = PyList_GetItem(PyTuple_GetItem(callfunc, 1), i);
        itemstList.push_back(PyUnicode_AsUTF8(pStartTime));
        PyObject *pEndTime = PyList_GetItem(PyTuple_GetItem(callfunc, 2), i);
        itemetList.push_back(PyUnicode_AsUTF8(pEndTime));
    }

    // Finalize Python interpreter
    Py_Finalize();

    //std::cout << "Device index: " << deviceIndex << std::endl;    
    std::cout << "Recorded file path: " << recordedFilePath << std::endl;
    std::cout << "Recording ended at: " << recordingEndTime << std::endl;
    std::cout << "Audio File uploaded to: " << uploadedAudio << std::endl;
    std::cout << "URI of transcribe: " << transcriptionURI << std::endl;
    std::cout << "Downloaded file path: " << downloadedFilePath << std::endl;
    std::cout << "Transcription File uploaded to: " << uploadedJSON << std::endl;
    std::cout << "File and path: " << file_path << std::endl;
    std::cout << "itemlist length " << length << std::endl;

    for (std::string items: itemList ){
        std::cout << items<< std::endl;
        //std::cout << itemstList[i]<< std::endl;
        //std::cout << itemetList[i]<< std::endl;
    }

    return 0;
    }