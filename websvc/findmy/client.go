package findmy

type MultiReportRequest struct {
	HashedAdvertisementKeyIds []string `json:"ids"`
	Days                      int      `json:"days"`
}

type ReportResponse struct {
	DatePublishedUnixMillis int    `json:"datePublished"`
	EncryptedPayloadBase64  string `json:"payload"`
	Description             string `json:"description"`
	HashedAdvertisementKey  string `json:"id"`
	StatusCode              int    `json:"statusCode"`
}

type MultiReportResponse struct {
	Results []ReportResponse `json:"results"`
}

func (resp *MultiReportResponse) ToMap() map[string]ReportResponse {
	ret := make(map[string]ReportResponse)
	for _, res := range resp.Results {
		ret[res.HashedAdvertisementKey] = res
	}
	return ret
}

type FindMyClient struct {
	LocationReportHttpAddress string
	LocationPrivateKey        []byte
	LocationAdvertisementKey  []byte

	DataPrefixMagic []byte
	DataModemID     []byte
}
