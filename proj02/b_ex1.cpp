#include <fstream>
#include <iostream>
#include <sndfile.h>
#include <map>
#include <math.h>
#include "BitStream.hh"
#include "Golomb.hh"
#include <vector>
#include <string>

using namespace std;

vector< pair<int,int> > code_info;  //vector with pairs of code info (code length, bits used for padding)

int predictor(char* audio_file);
int folding(short residual);
int defolding(short n);
int calculate_m(int sum, int num_items);
void encode(int num_items, short* error_buffer, int m);
void decode(int m);
void histograms(short* buffer, short* error_buffer, int num_items);

int main(int argc, char* argv[]){

    if(argc != 2){
        cout << "WRONG NUMBER OF ARGUMENTS!" << endl;
        return 1;
    }
    //encoding
    int m = predictor(argv[1]);

    //decoding
    decode(m);

    return 1;
}
//reads audio file, creates single channel (avg of stereo), predicts and calculates
//the residual values and calculates optimal m; it then encodes the value
//with its respective golomb code and writes to file using made classes

int predictor(char* audio_file){
    
    SNDFILE* file;
    SF_INFO sfinfo;
    sfinfo.format=0;

    file=sf_open(audio_file,SFM_READ,&sfinfo);

    int num_items = (int)sfinfo.frames * sfinfo.channels;
    short* buffer = (short *)malloc(num_items * sizeof(short));
    short* mono_buffer = (short *)malloc(sfinfo.frames * sizeof(short));
    short* error_buffer = (short *)malloc(sfinfo.frames * sizeof(short));
    int sum = 0;
    int rd_data = sf_read_short(file, buffer, num_items);

    for(int i = 0; i < rd_data; i += 2){
        mono_buffer[i / 2] = (buffer[i] + buffer[i + 1]) / 2; 
    }

    for(int i = 0; i < sfinfo.frames; i++){
        int f_;
        if(i > 2){
            f_ = (3 * mono_buffer[i - 1]) - (3 * mono_buffer[i - 2]) + mono_buffer[i - 3];
        }else if(i == 2){
            f_ = (2 * mono_buffer[i - 1]) - mono_buffer[i - 2];
        }else if(i == 1){
            f_ = mono_buffer[i - 1]; 
        }else{
            f_ = 0;
        }  
        error_buffer[i] = mono_buffer[i] - f_;

        //folding
        error_buffer[i] = folding(error_buffer[i]);

        sum += error_buffer[i]; 
    }

    //calculating histograms and entropy
    histograms(mono_buffer, error_buffer, sfinfo.frames);
    //calculating optimal m
    int m = calculate_m(sum, sfinfo.frames);
    //encoding
    encode(sfinfo.frames, error_buffer, m);
    
    return m;
}

int folding(short residual){
    //folding to get positive values only
    if (residual >= 0) residual = residual * 2;   
    else residual = residual * (-2) - 1;

    return residual;
}

int defolding(short n){
    //defolding
    if(n % 2 == 0){
        n /= 2;
    }else{
        n = (n + 1) / (-2);
    }
    return n;
}

int calculate_m(int sum, int num_items){
    //calculating the mean to find the "optimal" m -> log2(log(2) * (mean / sqrt(2)))
    double mean = (double) sum / num_items; 
    int m = log2(log(2) * (mean / sqrt(2)));
    cout << "-----------MEAN----------" << endl;
    cout << "mean -> " << mean << endl;
    cout << "--------OPTIMAL M--------" << endl;  
    cout << "optimal m -> " << m << endl;

    return m;
}

void encode(int num_items, short* error_buffer, int m){
    //for each value in the error buffer, calculate the golomb code and write to file via bitstream
    Golomb g;
    BitStream bstream(" ", "golomb_output.txt");
    string code;

    for(int i = 0; i < num_items; i++){
        int padding = 0;
        code = g.EncodeNumbers(error_buffer[i], m);
        code.erase(remove(code.begin(), code.end(), ' '), code.end());
        while(code.length() % 8 != 0){
            code.insert(0, "0");
            padding++;
        }
        pair<int,int> p(code.length(), padding);
        code_info.push_back(p); //keeping code sizes and padding for decoding purposes
        bstream.writeBits(code);
    }
    
    bstream.close();
}
void decode(int m){
    //reads encoded file and decodes each code with the help of 
    //a vector composed by the length of each code by order
    //it decodes each code with length x, and "defolds" the integer
    //to get the real value
    BitStream bitstm("golomb_output.txt", " ");
    Golomb g;
    for(int i = 0; code_info.size(); i++){

        int length = code_info.at(i).first;
        int padding = code_info.at(i).second;

        string x = bitstm.readBits(length); //reads first code
        x.erase(0, padding); //removes padding

        int n = g.DecodeNumbers(x, m); //decodes given code
        n = defolding(n);  //unfolds/defolds number
        cout << n << endl;
        //TODO -- WRITE AUDIO FILE WITH DECODED VALUES
    }
    
}
void histograms(short* buffer, short* error_buffer, int num_items){
    //calculating histograms and respective entropies, being the error_buffer < buffer
    map<short,int> error_buff_histo;
    map<short,int> buff_histo;
    map<short,int>::iterator it;
    double entropy = 0;
    double p = 0;

    for(int i = 0; i < num_items; i++){
        error_buff_histo[error_buffer[i]]++;
        buff_histo[buffer[i]]++;
    }

    cout<<"--------BUFFER--------"<<endl;
    for(it = buff_histo.begin(); it != buff_histo.end(); it++){
        //cout << it->first << " " << it->second << endl;
        p = (double)it->second / (num_items);
        entropy += p * log(p);
    }
    cout << "ENTROPY -> " << entropy * -1 << endl;
    
    entropy = 0;
    p = 0;
    
    cout<<"--------error BUFFER--------"<<endl;
    for(it = error_buff_histo.begin(); it != error_buff_histo.end(); it++){
        //cout << it->first << " " << it->second << endl;
        p = (double)it->second / (num_items);
        entropy += p * log(p);
    }
    cout << "ENTROPY -> " << entropy * -1 << endl;
}

    // void lossyCoding(int frames, int nbits){
    //     // TODO calcular nbits otimo

    //     int ptr[frames];
    //     for(int i=0 ; i<frames ; i++){
    //         ptr[i]=(bufferMono[i] >>  nbits) << nbits;
    //     }
    // }