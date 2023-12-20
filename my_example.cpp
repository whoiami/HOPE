#include "encoder_factory.hpp"
#include <algorithm>

double getNow() {
  struct timeval tv;
  gettimeofday(&tv, 0);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

int myrandom (int i) { return rand()%i;}

int main() {
  //static const int kEncoderType = 2; // DoubleCharEncoder
  //static const char kKeyFilePath[] = "../datasets/words.txt";
  static const char kKeyFilePath[] = "../workloads/txn_wiki_zipfian";
  //static const int kNumKeys = 234369;
  static const int kNumKeys = 10000000;

  // load input key list
  // this input key list is already sorted
  std::vector<std::string> keys;
  int64_t total_key_len = 0;
  std::ifstream infile(kKeyFilePath);
  std::string key;
  int count = 0;
  while (infile.good() && count < kNumKeys) {
    infile >> key;
    keys.push_back(key);
    total_key_len += (key.length() * 8);
    count++;
  }

  std::cout << "Random_shuffel begin " << std::endl;
  unsigned seed = time(0);
  srand(seed);
  random_shuffle(keys.begin(), keys.end(), myrandom);

  std::cout << "Random_shuffel done first key is "<< keys[0] << std::endl;

  std::cout << "Encoder build begin " << std::endl;
  // sample x% of the keys for building HOPE
  int skip = 100; // 1%
  //int skip = 10; // 10%
  //int skip = 1; // 100%
  std::vector<std::string> key_samples;
  for (int i = skip / 2; i < (int)keys.size(); i += skip) {
    key_samples.push_back(keys[i]);
  }

  double start = getNow();
  double end = getNow();
  // build the encoder
  hope::DoubleCharEncoder *encoder = new hope::DoubleCharEncoder();
  //encoder->build(key_samples, 5000); // 2nd para is dictionary size limit
  encoder->build(key_samples, 65536); // 2nd para is dictionary size limit

  end = getNow();
  std::cout << "Encoder build done time: "<< end - start << std::endl;

  start = getNow();
  std::cout << "Encode begin " << std::endl;
  // compress all keys
  std::vector<std::pair<std::string, int>> enc_keys;
  int64_t total_enc_len = 0;
  uint8_t *buffer = new uint8_t[10240];
  for (int i = 0; i < (int)keys.size(); i++) {
    int bit_len = encoder->encode(keys[i], buffer);
    total_enc_len += bit_len;
    enc_keys.push_back(std::pair<std::string, int>(std::string((const char *)buffer, (bit_len + 7) / 8), bit_len));
  }

  end = getNow();
  std::cout << "Encode done time: "<< end - start << std::endl;

  std::cout << "Decode begin " << std::endl;
  start = getNow();

  // decompress all keys
  std::vector<std::string> dec_keys;
  for (int i = 0; i < (int)enc_keys.size(); i++) {
    int len = encoder->decode(enc_keys[i].first, enc_keys[i].second, buffer);
   if (buffer[len - 1] == 0) len--;
    std::string dec_str1 = std::string((const char *)buffer, len);
    dec_keys.push_back(dec_str1);
    int cmp = dec_str1.compare(keys[i]);
    if (cmp) {
      std::cout << "!!!!!!!!!!encode decode not equal" << std::endl;
      assert(0);
      return -1;
    }
  }

  end = getNow();
  std::cout << "Decode done time: "<< end - start << std::endl;

  std::cout << "Before Compression total_key_len " << total_key_len << " after compression total_enc_len " << total_enc_len << std::endl;
  double cpr_rate =  total_key_len / (total_enc_len + 0.0);
  std::cout << "Compression Rate = " << cpr_rate << std::endl;

  start = getNow();
  std::cout << "Verify order-preserving property begin "<< std::endl;
  // verify the order-preserving property of HOPE
  for (int i = 0; i < (int)keys.size() - 1; i++) {
    int keys_cmp = keys[i].compare(keys[i + 1]);
    int cmp = enc_keys[i].first.compare(enc_keys[i + 1].first);
    if ((cmp > 0 && keys_cmp > 0)
      || (cmp < 0 && keys_cmp < 0)
      || (cmp == 0 && keys_cmp == 0)) {
    } else {
      std::cout << "!!!!!!!!!!!!!!Order-Preserving property violated!" << std::endl;
      assert(0);
      return -1;
    }
  }
  end = getNow();
  std::cout << "Verify order-preserving property done time: "<< end - start << std::endl;

  start = getNow();
  std::cout << "Sort keys and enc_keys and compare order begin "<< std::endl;
  struct {
    bool operator()(std::pair<std::string, int> a, std::pair<std::string, int> b) const { return a.first < b.first; }
  } customLess;
  std::sort(keys.begin(), keys.end());
  std::sort(enc_keys.begin(), enc_keys.end(), customLess);

  for (int i = 0; i < (int)enc_keys.size(); i++) {
    int len = encoder->decode(enc_keys[i].first, enc_keys[i].second, buffer);
   if (buffer[len - 1] == 0) len--;
    std::string dec_str1 = std::string((const char *)buffer, len);
    int cmp = dec_str1.compare(keys[i]);
    if (cmp) {
      std::cout << "!!!!!!!!!!encode decode not equal" << std::endl;
      assert(0);
      return -1;
    }
  }
  end = getNow();
  std::cout << "Sort keys and enc_keys and compare order end  time: "<< end - start << std::endl;

  return 0;
}
