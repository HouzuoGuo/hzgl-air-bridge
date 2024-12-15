package findmy

import (
	"bytes"
	"context"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"io"
	"net/http"
	"slices"
	"sort"
)

// MultiReportRequest is the location report request payload for hzgl/air-bridge-ws (Apple Find My HTTP service adapter).
type MultiReportRequest struct {
	HashedAdvertisedPublicKey []string `json:"ids"`
	Days                      int      `json:"days"`
}

type ReportResponse struct {
	DatePublishedUnixMillis   int64  `json:"datePublished"`
	EncryptedPayloadBase64    string `json:"payload"`
	Description               string `json:"description"`
	HashedAdvertisedPublicKey string `json:"id"`
	StatusCode                int    `json:"statusCode"`
}

func (resp ReportResponse) EncryptedPayload() []byte {
	decodedBytes, err := base64.StdEncoding.DecodeString(resp.EncryptedPayloadBase64)
	if err != nil {
		return nil
	}
	return decodedBytes
}

// MultiReportRequest is the location report response payload.
type MultiReportResponse struct {
	Results []ReportResponse `json:"results"`
}

// ToMap returns a map of hashed advertisement public key to its latest location report.
func (resp *MultiReportResponse) ToMap() map[string]ReportResponse {
	ret := make(map[string]ReportResponse)
	for _, res := range resp.Results {
		existing, ok := ret[res.HashedAdvertisedPublicKey]
		if !ok || existing.DatePublishedUnixMillis < res.DatePublishedUnixMillis {
			ret[res.HashedAdvertisedPublicKey] = res
		}
	}
	return ret
}

// Client is a HTTP client of hzgl/air-bridge-ws (Apple Find My HTTP service adapter).
type Client struct {
	httpClient                *http.Client
	LocationReportHttpAddress string
	LocationPrivateKey        []byte
	LocationAdvertisementKey  []byte

	PubkeyMagic1 int
	PubkeyMagic2 int
	ModemID      int
}

func NewClient(reportHttpAddr string, locPrivKey, locAdvertKey []byte, pubkeyMagic1, pubkeyMagic2, modemID int) *Client {
	return &Client{
		httpClient:                &http.Client{},
		LocationReportHttpAddress: reportHttpAddr,
		LocationPrivateKey:        locPrivKey,
		LocationAdvertisementKey:  locAdvertKey,
		PubkeyMagic1:              pubkeyMagic1,
		PubkeyMagic2:              pubkeyMagic2,
		ModemID:                   modemID,
	}
}

func (client *Client) doReportRequest(ctx context.Context, req MultiReportRequest) (*MultiReportResponse, error) {
	reportReqBody, err := json.Marshal(req)
	if err != nil {
		return nil, fmt.Errorf("failed to serialise report request: %w", err)
	}
	httpReq, err := http.NewRequestWithContext(ctx, http.MethodPost, client.LocationReportHttpAddress, bytes.NewReader(reportReqBody))
	if err != nil {
		return nil, fmt.Errorf("failed to construct http request: %w", err)
	}
	httpReq.Header.Set("Content-Type", "application/json")
	httpResp, err := client.httpClient.Do(httpReq)
	if err != nil {
		return nil, fmt.Errorf("http request failure: %w", err)
	}
	respBody, err := io.ReadAll(httpResp.Body)
	if err != nil {
		return nil, fmt.Errorf("failed to read http response: %w", err)
	}
	var resp MultiReportResponse
	err = json.Unmarshal(respBody, &resp)
	if err != nil {
		return nil, fmt.Errorf("failed to deserialise http response: %w", err)
	}
	// Remove invalid reports. I am unsure why they occasionally show up.
	slices.DeleteFunc(resp.Results, func(e ReportResponse) bool {
		return e.EncryptedPayloadBase64 == "" || e.DatePublishedUnixMillis <= 1
	})
	// Sort retrieved location reports from latest to oldest.
	sort.Slice(resp.Results, func(a, b int) bool {
		return resp.Results[a].DatePublishedUnixMillis > resp.Results[b].DatePublishedUnixMillis
	})
	return &resp, nil
}
