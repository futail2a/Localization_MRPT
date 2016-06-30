#include "MonteCarloLocalization.h"

using namespace ssr;

std::vector<std::string> extractParticleParams(mrpt::slam::CMonteCarloLocalization2D &pdf_){
	std::vector<std::string> particle_params;
	for (int i = 0; i < pdf_.m_particles.size(); i+=4){
		particle_params[i] = std::to_string(pdf_.m_particles[i].d->x());
		particle_params[i + 1] = std::to_string(pdf_.m_particles[i].d->y());
		particle_params[i + 2] = std::to_string(pdf_.m_particles[i].d->phi());
		particle_params[i + 3] = std::to_string(pdf_.m_particles[i].log_w);
	}
	return particle_params;
}
void setParticleParams(std::vector<std::string> particle_params, mrpt::slam::CMonteCarloLocalization2D &pdf_){
	std::size_t ei;
	for (int i = 0; i < pdf_.m_particles.size(); i+=4){
		pdf_.m_particles[i].d->x(atof(particle_params[i].c_str()));
		pdf_.m_particles[i + 1].d->y(atof(particle_params[i + 1].c_str()));
		pdf_.m_particles[i + 2].d->phi(atof(particle_params[i + 2].c_str()));
		pdf_.m_particles[i + 3].log_w = atof(particle_params[i + 3].c_str());
	}
}

MCLocalization_MRPT::MCLocalization_MRPT(){
	//constractor 
}

MCLocalization_MRPT::~MCLocalization_MRPT(){
	//destractor 
}

void  MCLocalization_MRPT::setMap(const RTC::OGMap& map){
	OGMap2COccupancyGridMap(map, &m_map);
}

void  MCLocalization_MRPT::initialize(){
	pfOptions_.PF_algorithm = m_PF_algorithm;
	pfOptions_.resamplingMethod = m_resamplingMethod;
	pfOptions_.adaptiveSampleSize = m_adaptiveSampleSize;
	pfOptions_.pfAuxFilterOptimal_MaximumSearchSamples = m_pfAuxFilterOptimal_MaximumSearchSamples;
	pfOptions_.BETA = m_BETA;
	pfOptions_.sampleSize = m_sampleSize;
    pfOptions_.dumpToConsole();
 
    pf_.m_options = pfOptions_;
	
	///
	pdf_.clear();
	pdf_.options.KLD_params.KLD_binSize_PHI =		m_KLD_binSize_PHI;
	pdf_.options.KLD_params.KLD_binSize_XY =		m_KLD_binSize_XY;
	pdf_.options.KLD_params.KLD_delta =				m_KLD_delta;
	pdf_.options.KLD_params.KLD_epsilon =			m_KLD_epsilon;
	pdf_.options.KLD_params.KLD_maxSampleSize =		m_KLD_maxSampleSize;
	pdf_.options.KLD_params.KLD_minSampleSize =		m_KLD_minSampleSize;
	pdf_.options.KLD_params.KLD_minSamplesPerBin =	m_KLD_minSamplesPerBin;
	 
	// m_resolution
	m_map.insertionOptions.mapAltitude = m_mapAltitude;
	m_map.insertionOptions.useMapAltitude = m_useMapAltitude;
	m_map.insertionOptions.maxDistanceInsertion = m_maxDistanceInsertion;
	m_map.insertionOptions.maxOccupancyUpdateCertainty = m_maxOccupancyUpdateCertainty;
	m_map.insertionOptions.considerInvalidRangesAsFreeSpace = m_considerInvalidRangesAsFreeSpace;
	m_map.insertionOptions.decimation = m_decimation;
	m_map.insertionOptions.horizontalTolerance =  m_horizontalTolerance;
	m_map.insertionOptions.CFD_features_gaussian_size = m_CFD_features_gaussian_size;
	m_map.insertionOptions.CFD_features_median_size = m_CFD_features_median_size;
	m_map.insertionOptions.wideningBeamsWithDistance = m_wideningBeamsWithDistance;
	m_map.insertionOptions.dumpToConsole();
	
	m_map.likelihoodOptions.likelihoodMethod = m_likelihoodMethod;	
	m_map.likelihoodOptions.enableLikelihoodCache = m_enableLikelihoodCache;
	m_map.likelihoodOptions.LF_decimation= m_LF_decimation;
	m_map.likelihoodOptions.LF_stdHit= m_LF_stdHit;
	m_map.likelihoodOptions.LF_maxCorrsDistance=  m_LF_maxCorrsDistance;
	m_map.likelihoodOptions.LF_zHit= m_LF_zHit;
	m_map.likelihoodOptions.LF_zRandom=  m_LF_zRandom;
	m_map.likelihoodOptions.LF_maxRange=  m_LF_maxRange;
	m_map.likelihoodOptions.LF_alternateAverageMethod= m_LF_alternateAverageMethod;
	m_map.likelihoodOptions.MI_exponent= m_MI_exponent;
	m_map.likelihoodOptions.MI_skip_rays= m_MI_skip_rays;
	m_map.likelihoodOptions.MI_ratio_max_distance= m_MI_ratio_max_distance;		
	m_map.likelihoodOptions.rayTracing_useDistanceFilter= m_rayTracing_useDistanceFilter;
	m_map.likelihoodOptions.rayTracing_decimation= m_rayTracing_decimation;
	m_map.likelihoodOptions.rayTracing_stdHit= m_rayTracing_stdHit;
	m_map.likelihoodOptions.consensus_takeEachRange= m_consensus_takeEachRange;
	m_map.likelihoodOptions.consensus_pow= m_consensus_pow;	
	m_map.likelihoodOptions.dumpToConsole();

	pdf_.options.metricMap = &m_map;

	///
	motion_model_options_.modelSelection = mrpt::obs::CActionRobotMovement2D::mmGaussian;
	motion_model_options_.gausianModel.minStdXY  = m_minStdXY;
	motion_model_options_.gausianModel.minStdPHI = m_minStdPHI;

	//pdf_.resetUniformFreeSpace(&m_map, 0.7f, m_particles_count, m_min_x, m_max_x, m_min_y, m_max_y);//, m_min_phi, m_max_phi);
	pdf_.resetUniform(m_map.getXMin(), m_map.getXMax(),
			  m_map.getYMin(), m_map.getYMax(),
			  -M_PI, M_PI, m_particles_count);//, m_min_phi, m_max_phi);

	//m_particles = ObjectToString(pdf_);
}

bool MCLocalization_MRPT::addPose(const ssr::Pose2D& deltaPose)
{
    mrpt::obs::CActionRobotMovement2D action;
	mrpt::obs::CActionRobotMovement2D::TMotionModelOptions options;
	action.computeFromOdometry(mrpt::poses::CPose2D(deltaPose.x, deltaPose.y, deltaPose.th), motion_model_options_);
	action.timestamp = mrpt::system::getCurrentTime();
	static mrpt::system::TTimeStamp oldTimestamp;
	if(action.timestamp == oldTimestamp) {
		action.timestamp = oldTimestamp +1;
	}
	oldTimestamp = action.timestamp;
	m_ActionCollection.clear();
	m_ActionCollection.insert(action);
	return true;
}

bool MCLocalization_MRPT::addRange(const ssr::Range& range)
{
  mrpt::obs::CObservation2DRangeScanPtr observation = mrpt::obs::CObservation2DRangeScan::Create();
	observation->rightToLeft = true;
	observation->validRange.resize(range.size);
	observation->scan.resize(range.size);
	observation->aperture = range.aperture;
	observation->timestamp = mrpt::system::getCurrentTime();
	for(int i = 0;i < range.size; i++) {
		observation->scan[i] = range.range[i];
		if(observation->scan[i] > m_range_min && observation->scan[i] < m_range_max) {
			observation->validRange[i] = 1;
		} else {
			observation->validRange[i] = 0;
		}
	}
	observation->setSensorPose(m_RangeSensorPose);
	m_SensoryFrame.clear();
	m_SensoryFrame.insert(observation);
	return true;
}

mrpt::poses::CPose2D MCLocalization_MRPT::getEstimatedPose(){
	//StringToObject(m_particles, pdf_);
	//pdf_.copyFrom(m_particles);
	pf_.executeOn(pdf_, &m_ActionCollection, &m_SensoryFrame, &pf_stats_);
	//m_particles = ObjectToString(pdf_);
	return pdf_.getMeanVal();
}

void MCLocalization_MRPT::OGMap2COccupancyGridMap(RTC::OGMap ogmap, mrpt::maps::COccupancyGridMap2D *gridmap) {
	gridmap->setSize(
		-ogmap.config.origin.position.x,
		ogmap.map.width * ogmap.config.xScale - ogmap.config.origin.position.x,
		-(ogmap.map.height * ogmap.config.yScale + ogmap.config.origin.position.y),
		-ogmap.config.origin.position.y,
		ogmap.config.xScale);
	//gridmap->setSize(0-ogmap.map.width*ogmap.config.xScale/2, ogmap.map.width*ogmap.config.xScale/2, 0-ogmap.map.width*ogmap.config.yScale/2, ogmap.map.height*ogmap.config.yScale/2, ogmap.config.xScale);
	int height = gridmap->getSizeY();
	int width =  gridmap->getSizeX();

	for(int i=0; i <height ; i++){
		for(int j=0; j <width ; j++){
			int cell = ogmap.map.cells[(height-i-1) * width + j];
	
			if(cell < 100){
				gridmap->setCell(j, i, 0.0);
			}else if(cell > 200){
				gridmap->setCell(j, i, 1.0);
			}else{
				//gridmap->setCell(j, i, 0.5);���m�̈���O���[��
				gridmap->setCell(j, i, 1.0);//���m�̈�𔒂�
			}
		}
	}
	/*
	CImage		img;
	gridmap->getAsImage(img,false, true);  
	mrpt::gui::CDisplayWindow	win("Computed path");
	win.showImage(img.scaleDouble().scaleDouble());
	win.waitForKey();
	//*/
}

void MCLocalization_MRPT::TimedPose2D2CPose2D(const RTC::TimedPose2D & tp, mrpt::poses::CPose2D & cp, const RTC::OGMap & map){
		cp.x(map.map.column + tp.data.position.x / map.config.yScale);
		cp.y(map.map.row + tp.data.position.y / map.config.yScale);
		cp.phi(tp.data.heading);
	}
