/* converter
 *
 * convert peakfile into a standard format:
 *      chromID  start  length  count
 * and save into a vector
 *
 * Last modified by An Zheng
 */
#include "peak_loader.h"

const std::map<std::string, int> PeakLoader::peakfileTypeList = {{"homer", 0}, {"wce", 1}, {"bed", 2}};

PeakLoader::PeakLoader(const std::string _peakfile, const std::string _peakfileType,
                        const std::string _bamfile, const std::int32_t _count_colidx){
  if (_peakfileType == "") {
    std::cerr << "****** ERROR: Need to specify the type of the peak file ******" << std::endl;
    std::exit(1);
  } else if ((_bamfile == "") && (_count_colidx == -1) && (_peakfileType != "wce")) {
    std::cerr << "****** ERROR: Need to specify either parameter -c or parameter -b ******" << std::endl;
    std::exit(1);
  } else {
    peakfile = _peakfile;
    peakfileType = _peakfileType;
    bamfile = _bamfile;
    count_colidx = _count_colidx;

    // unify names of peak file types into lower cases
    std::transform(_peakfileType.begin(), _peakfileType.end(), peakfileType.begin(), ::tolower);
  }
}

bool PeakLoader::Load(std::vector<Fragment>& peaks, const std::string region, const float frag_length,
		      const bool noscale, const bool scale_outliers){
  PeakReader peakreader(peakfile);
  switch (peakfileTypeList.at(peakfileType)) {
  case 0:
    peakreader.HomerPeakReader(peaks, count_colidx, region, noscale, scale_outliers);
    break;
  case 1:
    peakreader.EmptyPeakReader();
    break;
  case 2:
    peakreader.BedPeakReader(peaks, count_colidx, region, noscale, scale_outliers);
    break;
  default:
    std::cerr << "An unexpected error happened in PeakLoader->Load(). Invalid peak type specified. Options are bed, homer, or wce" << std::endl;
    return false;
    break;
  }

  if (bamfile != ""){
    peakreader.UpdateTagCount(peaks, bamfile, &total_genome_length, &total_tagcount, &tagcount_in_peaks,
			      region, frag_length, noscale, scale_outliers);
  }
  return true;
}
