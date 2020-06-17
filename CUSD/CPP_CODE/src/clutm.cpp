#include "include.h"
#include "clutm.h"
using namespace std;

void clutm::PrintProgress (int done, int total)
{
	real percentage = (real)done/total;
    int val = (int) (percentage * 100);
    int lpad = (int) (percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf ("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
    fflush (stdout);
}


void clutm::write_characteristic_dist(ofstream& outfile) {
	int i, j;
	vector<vector<real> > psi = GetCharacteristicWordDist();

	for(i=0; i<psi.size(); i++){
		for(j=0; j<this->vocab_size - 1; j++) {
			outfile << psi[i][j]<<"\t";
		}
		outfile << psi[i][this->vocab_size-1]<<"\n";
	}
}


void clutm::write_topic_dist(ofstream& outfile) {
	int i, j;
	vector<vector<real> > chi = GetTopicCharacteristicDist();

	for(i=0; i<this->num_topics; i++){
		for(j=0; j<this->num_characteristics - 1; j++) {
			outfile << chi[i][j]<<"\t";
		}
		outfile << chi[i][this->num_characteristics-1]<<"\n";
	}
}


void clutm::write_topic_query_dist(ofstream& outfile) {
	int i, j;
	vector<real> phi = GetTopicDist();

	for(i=0; i<this->num_topics - 1; i++){
		outfile << phi[i]<<"\t";
	}
	outfile << phi[this->num_topics-1]<<"\n";
}


void clutm::read_characteristic_dist(ifstream& infile) {
	int i, j;
	this->trained_psi.resize(this->num_characteristics + this->null_tag, vector<real>(this->vocab_size));

	for(i=0; i<trained_psi.size(); i++){
		for(j=0; j<this->vocab_size; j++) {
			infile >> trained_psi[i][j];
		}
	}
}

void clutm::read_topic_dist(ifstream& infile) {
	int i, j;
	this->trained_chi.resize(this->num_topics, vector<real>(this->num_characteristics));

	for(i=0; i<this->num_topics; i++){
		for(j=0; j<this->num_characteristics; j++) {
			infile >> trained_chi[i][j];
		}
	}
}

void clutm::read_topic_query_dist(ifstream& infile) {
	int i, j;
	this->trained_phi.resize(this->num_topics);

	for(i=0; i<this->num_topics; i++){
		infile >> trained_phi[i];
	}
}

void clutm::Inference(int train) {

	int i, j, k;
	int index;
	int current_characteristic, new_characteristic;
	int current_topic, new_topic;
	real max_prob, temp_prob;
	query *q;
	int word;
	vector<int> indices(this->queries.size());

	for(i=0; i<this->queries.size(); i++) {
		indices[i] = i;
	}

	random_shuffle(indices.begin(), indices.end());


	for(i=0; i<indices.size(); i++) {
		index = indices[i];
		q = &(this->queries[index]);

		//First find the topic of the query
		current_topic = q->my_topic;

		if(train == 1) {
			this->topic_query_count[current_topic]--;
			this->topic_count[current_topic] -= (q->my_label_size - this->null_tag);
			for (j=0; j<q->my_label_size-this->null_tag; j++) {
				this->topic_characteristic_count[current_topic][q->labels[j]]--;
			}
		}

		if(train == 1) {

			for (k=0; k<this->num_topics; k++) {
				q->topic_weights[k] = this->alpha + this->topic_query_count[k];

				for (j=0; j<q->my_label_size-this->null_tag; j++) {
					q->topic_weights[k] *= (this->beta + this->topic_characteristic_count[k][q->labels[j]])/(this->beta*this->num_characteristics + this->topic_count[k] + j);
				}
			}
		}

		else {

			for (k=0; k<this->num_topics; k++) {
				q->topic_weights[k] = this->trained_phi[k];

				for (j=0; j<q->my_label_size-this->null_tag; j++) {
					q->topic_weights[k] *= this->trained_chi[k][q->labels[j]];
				}
			}
		}

		this->distribution = discrete_distribution<int>(q->topic_weights.begin(),q->topic_weights.end());
		new_topic = this->distribution(this->engine);

		q->my_topic = new_topic;


		if(train == 1) {
			this->topic_query_count[new_topic]++;
			this->topic_count[new_topic] += q->my_label_size - this->null_tag;
			for (j=0; j<q->my_label_size-this->null_tag; j++) {
				this->topic_characteristic_count[new_topic][q->labels[j]]++;
			}
		}

		//Find the characteristic for every word in the query
		for(j=0; j<q->length; j++) {
			current_characteristic = q->characteristic_assign[j];
			word = q->words[j];
			if(train == 1) {
				temp_prob = (this->characteristic_word_count[q->labels[current_characteristic]][word] - 1 + this->delta)/(this->characteristic_count[q->labels[current_characteristic]] - 1 + this->vocab_size * this->delta);
			}
			else {
				temp_prob = this->trained_psi[q->labels[current_characteristic]][word];
			}
			q->characteristic_weights[current_characteristic] = temp_prob;

			for(k=0; k<q->my_label_size; k++) {
				if(k == current_characteristic){
					continue;
				}

				if(train == 1) {
					temp_prob = (this->characteristic_word_count[q->labels[k]][word] + this->delta)/(this->characteristic_count[q->labels[k]] + this->vocab_size * this->delta);
				}
				else {
					temp_prob = this->trained_psi[q->labels[k]][word];
				}
				q->characteristic_weights[k] = temp_prob;
			}
			this->distribution = discrete_distribution<int>(q->characteristic_weights.begin(),q->characteristic_weights.end());
			new_characteristic = this->distribution(this->engine);

			if(current_characteristic != new_characteristic) {
				q->characteristic_assign[j] = new_characteristic;
				q->characteristic_count[current_characteristic]--;
				q->characteristic_count[new_characteristic]++;

				if(train == 1){
					this->characteristic_word_count[q->labels[current_characteristic]][word]--;
					this->characteristic_count[q->labels[current_characteristic]]--;

					this->characteristic_word_count[q->labels[new_characteristic]][word]++;
					this->characteristic_count[q->labels[new_characteristic]]++;
				}

			}

		}
		if(i%1000 == 0){
			PrintProgress(i+1, indices.size());
		}
	}
	PrintProgress(indices.size(), indices.size());
	cout<<endl;



	// for(int l=0; l<this->num_topics; l++) {
	// 	int temp = 0;
	// 	for (j=0; j<this->num_characteristics; j++) {
	// 		temp += this->topic_characteristic_count[l][j];
	// 	}
	// 	cout<<this->topic_count[l]<<" "<<temp<<endl;
	// 	cout<<this->topic_query_count[l]<<endl;
	// }
}

vector<vector<real> > clutm::GetCharacteristicWordDist(){
	int i,j;
	real denom;
	vector<vector<real> > psi(this->num_characteristics+this->null_tag, vector<real>(this->vocab_size));
	for(i=0; i<psi.size(); i++){
		denom = this->characteristic_count[i] + this->vocab_size * this->delta;
		for(j=0; j<this->vocab_size; j++) {
			psi[i][j] = (this->characteristic_word_count[i][j] + this->delta)/denom;
		}
	}
	return psi;
}

vector<vector<real> > clutm::GetTopicCharacteristicDist(){
	int i,j;
	real denom;
	vector<vector<real> > chi(this->num_topics, vector<real>(this->num_characteristics));
	for(i=0; i<this->num_topics; i++){
		denom = this->topic_count[i] + this->num_characteristics * this->beta;
		for(j=0; j<this->num_characteristics; j++) {
			chi[i][j] = (this->topic_characteristic_count[i][j] + this->beta)/denom;
		}
	}
	return chi;
}

vector<real> clutm::GetTopicDist(){
	int i;
	real denom;
	vector<real> phi(this->num_topics);
	denom = this->queries.size() + this->num_topics * this->alpha;
	for(i=0; i<this->num_topics; i++){
		phi[i] = (this->topic_query_count[i] + this->alpha)/denom;
	}
	return phi;
}

real clutm::Perplexity(int train) {
	int i, j, k;
	int cnt = 0;
	int word = 0;
	real perplexity = 0.0;
	real dot_product = 0.0;
	vector<vector<real> > psi;
	if (train == 1){
		psi = GetCharacteristicWordDist();
	}
	else {
		psi = this->trained_psi;
	}
	
	for(i=0; i<this->queries.size(); i++) {
		for(j=0; j<this->queries[i].length; j++) {
			word = this->queries[i].words[j];
			dot_product = 0.0;
			for(k=0; k<this->queries[i].my_label_size; k++) {
				dot_product += psi[this->queries[i].labels[k]][word] / queries[i].my_label_size;
			}
			perplexity -= log(dot_product);
		}
		cnt += this->queries[i].length;
	}
	return exp(perplexity/cnt);
}

void clutm::InitializeCounts() {
	int i, j;
	for(i=0; i<this->queries.size(); i++) {
		for(j=0; j<queries[i].characteristic_assign.size(); j++) {
			this->characteristic_word_count[queries[i].labels[queries[i].characteristic_assign[j]]][queries[i].words[j]] += 1;
			this->characteristic_count[queries[i].labels[queries[i].characteristic_assign[j]]] += 1;
		}

		this->topic_query_count[queries[i].my_topic] += 1;
		this->topic_count[queries[i].my_topic] += queries[i].my_label_size - this->null_tag;
		for(j=0; j<queries[i].my_label_size - this->null_tag; j++) {
			this->topic_characteristic_count[queries[i].my_topic][queries[i].labels[j]] += 1;
		}
	}
}


clutm::clutm(int vocab_size, int num_characteristics, int num_topics, vector<query> &queries, int num_threads, real alpha, real beta, real delta, int null_tag, int seed):vocab_size(vocab_size),num_characteristics(num_characteristics),num_topics(num_topics),queries(queries),num_threads(num_threads),alpha(alpha),beta(beta),delta(delta), null_tag(null_tag), seed(seed) {
	int i;
	this->engine.seed(this->seed);

	this->topic_query_count.resize(this->num_topics, 0);
	this->topic_count.resize(this->num_topics, 0);
	this->topic_characteristic_count.resize(this->num_topics);

	for (i=0; i<this->num_topics; i++) {
		this->topic_characteristic_count[i].resize(this->num_characteristics, 0);
	}

	if(this->null_tag == 0) {

		this->characteristic_count.resize(this->num_characteristics, 0);
		this->characteristic_word_count.resize(this->num_characteristics);

		for (i=0; i<this->num_characteristics; i++) {
			this->characteristic_word_count[i].resize(this->vocab_size, 0);
		}
		
	}

	else {

		this->characteristic_count.resize(this->num_characteristics+1, 0);
		this->characteristic_word_count.resize(this->num_characteristics+1);

		for (i=0; i<this->num_characteristics+1; i++) {
			this->characteristic_word_count[i].resize(this->vocab_size, 0);
		}
		
	}
	InitializeCounts();
}

clutm::~clutm() {
}
